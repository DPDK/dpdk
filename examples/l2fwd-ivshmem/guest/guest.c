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
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <rte_common.h>
#include <rte_eal_memconfig.h>
#include <rte_log.h>
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
#include <rte_ivshmem.h>

#include "../include/common.h"

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table rx_mbufs[RTE_MAX_ETHPORTS];
	struct vm_port_param * port_param[MAX_RX_QUEUE_PER_LCORE];
} __rte_cache_aligned;
static struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");

	for (portid = 0; portid < ctrl->nb_ports; portid++) {
		/* skip ports that are not enabled */
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   ctrl->vm_ports[portid].stats.tx,
			   ctrl->vm_ports[portid].stats.rx,
			   ctrl->vm_ports[portid].stats.dropped);

		total_packets_dropped += ctrl->vm_ports[portid].stats.dropped;
		total_packets_tx += ctrl->vm_ports[portid].stats.tx;
		total_packets_rx += ctrl->vm_ports[portid].stats.rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_packets_dropped);
	printf("\n====================================================\n");
}

/* display usage */
static void
l2fwd_ivshmem_usage(const char *prgname)
{
	printf("%s [EAL options] -- [-q NQ -T PERIOD]\n"
		   "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static unsigned int
l2fwd_ivshmem_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_ivshmem_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_ivshmem_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "q:p:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {

		/* nqueue */
		case 'q':
			l2fwd_ivshmem_rx_queue_per_lcore = l2fwd_ivshmem_parse_nqueue(optarg);
			if (l2fwd_ivshmem_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_ivshmem_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_ivshmem_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_ivshmem_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			l2fwd_ivshmem_usage(prgname);
			return -1;

		default:
			l2fwd_ivshmem_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

/*
 * this loop is getting packets from RX rings of each port, and puts them
 * into TX rings of destination ports.
 */
static void
fwd_loop(void)
{

	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf **m_table;
	struct rte_mbuf *m;
	struct rte_ring *rx, *tx;
	unsigned lcore_id, len;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	unsigned i, j, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	struct ether_hdr *eth;
	void *tmp;

	prev_tsc = 0;
	timer_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD_IVSHMEM, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD_IVSHMEM, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {
		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD_IVSHMEM, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (ctrl->state == STATE_FWD) {
		cur_tsc = rte_rdtsc();

		diff_tsc = cur_tsc - prev_tsc;

		/*
		 * Read packet from RX queues and send it to TX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {

			portid = qconf->rx_port_list[i];

			len = qconf->rx_mbufs[portid].len;

			rx = ctrl->vm_ports[portid].rx_ring;
			tx = ctrl->vm_ports[portid].dst->tx_ring;

			m_table = qconf->rx_mbufs[portid].m_table;

			/* if we have something in the queue, try and transmit it down */
			if (len != 0) {

				/* if we succeed in sending the packets down, mark queue as free */
				if (rte_ring_enqueue_bulk(tx, (void**) m_table, len) == 0) {
					ctrl->vm_ports[portid].stats.tx += len;
					qconf->rx_mbufs[portid].len = 0;
					len = 0;
				}
			}

			nb_rx = rte_ring_count(rx);

			nb_rx = RTE_MIN(nb_rx, (unsigned) MAX_PKT_BURST);

			if (nb_rx == 0)
				continue;

			/* if we can get packets into the m_table */
			if (nb_rx < (RTE_DIM(qconf->rx_mbufs[portid].m_table) - len)) {

				/* this situation cannot exist, so if we fail to dequeue, that
				 * means something went horribly wrong, hence the failure. */
				if (rte_ring_dequeue_bulk(rx, (void**) pkts_burst, nb_rx) < 0) {
					ctrl->state = STATE_FAIL;
					return;
				}

				ctrl->vm_ports[portid].stats.rx += nb_rx;

				/* put packets into the queue */
				for (j = 0; j < nb_rx; j++) {
					m = pkts_burst[j];

					rte_prefetch0(rte_pktmbuf_mtod(m, void *));

					m_table[len + j] = m;

					eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

					/* 02:00:00:00:00:xx */
					tmp = &eth->d_addr.addr_bytes[0];
					*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)portid << 40);

					/* src addr */
					ether_addr_copy(&ctrl->vm_ports[portid].dst->ethaddr,
							&eth->s_addr);
				}
				qconf->rx_mbufs[portid].len += nb_rx;

			}

		}

		/* if timer is enabled */
		if (timer_period > 0) {

			/* advance the timer */
			timer_tsc += diff_tsc;

			/* if timer has reached its timeout */
			if (unlikely(timer_tsc >= (uint64_t) timer_period)) {

				/* do this only on master core */
				if (lcore_id == rte_get_master_lcore()) {
					print_stats();
					/* reset the timer */
					timer_tsc = 0;
				}
			}
		}

		prev_tsc = cur_tsc;
	}
}

static int
l2fwd_ivshmem_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	fwd_loop();
	return 0;
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	const struct rte_memzone * mz;
	int ret;
	uint8_t portid;
	unsigned rx_lcore_id, lcore_id;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_ivshmem_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid l2fwd-ivshmem arguments\n");

	/* find control structure */
	mz = rte_memzone_lookup(CTRL_MZ_NAME);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot find control memzone\n");

	ctrl = (struct ivshmem_ctrl*) mz->addr;

	/* lock the ctrl so that we don't have conflicts with anything else */
	rte_spinlock_lock(&ctrl->lock);

	if (ctrl->state == STATE_FWD)
		rte_exit(EXIT_FAILURE, "Forwarding already started!\n");

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < ctrl->nb_ports; portid++) {

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
			   lcore_queue_conf[rx_lcore_id].n_rx_port ==
			   l2fwd_ivshmem_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->port_param[qconf->n_rx_port] = &ctrl->vm_ports[portid];
		qconf->n_rx_port++;

		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	sigsetup();

	/* indicate that we are ready to forward */
	ctrl->state = STATE_FWD;

	/* unlock */
	rte_spinlock_unlock(&ctrl->lock);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_ivshmem_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
