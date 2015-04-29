/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <getopt.h>

#include <rte_alarm.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
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
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>

#include <rte_errno.h>
#include <rte_jobstats.h>
#include <rte_timer.h>
#include <rte_alarm.h>

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

#define UPDATE_STEP_UP 1
#define UPDATE_STEP_DOWN 32

static unsigned int l2fwd_rx_queue_per_lcore = 1;

struct mbuf_table {
	uint64_t next_flush_time;
	unsigned len;
	struct rte_mbuf *mbufs[MAX_PKT_BURST];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

	struct rte_timer rx_timers[MAX_RX_QUEUE_PER_LCORE];
	struct rte_jobstats port_fwd_jobs[MAX_RX_QUEUE_PER_LCORE];

	struct rte_timer flush_timer;
	struct rte_jobstats flush_job;
	struct rte_jobstats idle_job;
	struct rte_jobstats_context jobs_context;

	rte_atomic16_t stats_read_pending;
	rte_spinlock_t lock;
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

struct rte_mempool *l2fwd_pktmbuf_pool = NULL;

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

/* 1 day max */
#define MAX_TIMER_PERIOD 86400
/* default period is 10 seconds */
static int64_t timer_period = 10;
/* default timer frequency */
static double hz;
/* BURST_TX_DRAIN_US converted to cycles */
uint64_t drain_tsc;
/* Convert cycles to ns */
static inline double
cycles_to_ns(uint64_t cycles)
{
	double t = cycles;

	t *= (double)NS_PER_S;
	t /= hz;
	return t;
}

static void
show_lcore_stats(unsigned lcore_id)
{
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	struct rte_jobstats_context *ctx = &qconf->jobs_context;
	struct rte_jobstats *job;
	uint8_t i;

	/* LCore statistics. */
	uint64_t stats_period, loop_count;
	uint64_t exec, exec_min, exec_max;
	uint64_t management, management_min, management_max;
	uint64_t busy, busy_min, busy_max;

	/* Jobs statistics. */
	const uint8_t port_cnt = qconf->n_rx_port;
	uint64_t jobs_exec_cnt[port_cnt], jobs_period[port_cnt];
	uint64_t jobs_exec[port_cnt], jobs_exec_min[port_cnt],
				jobs_exec_max[port_cnt];

	uint64_t flush_exec_cnt, flush_period;
	uint64_t flush_exec, flush_exec_min, flush_exec_max;

	uint64_t idle_exec_cnt;
	uint64_t idle_exec, idle_exec_min, idle_exec_max;
	uint64_t collection_time = rte_get_timer_cycles();

	/* Ask forwarding thread to give us stats. */
	rte_atomic16_set(&qconf->stats_read_pending, 1);
	rte_spinlock_lock(&qconf->lock);
	rte_atomic16_set(&qconf->stats_read_pending, 0);

	/* Collect context statistics. */
	stats_period = ctx->state_time - ctx->start_time;
	loop_count = ctx->loop_cnt;

	exec = ctx->exec_time;
	exec_min = ctx->min_exec_time;
	exec_max = ctx->max_exec_time;

	management = ctx->management_time;
	management_min = ctx->min_management_time;
	management_max = ctx->max_management_time;

	rte_jobstats_context_reset(ctx);

	for (i = 0; i < port_cnt; i++) {
		job = &qconf->port_fwd_jobs[i];

		jobs_exec_cnt[i] = job->exec_cnt;
		jobs_period[i] = job->period;

		jobs_exec[i] = job->exec_time;
		jobs_exec_min[i] = job->min_exec_time;
		jobs_exec_max[i] = job->max_exec_time;

		rte_jobstats_reset(job);
	}

	flush_exec_cnt = qconf->flush_job.exec_cnt;
	flush_period = qconf->flush_job.period;
	flush_exec = qconf->flush_job.exec_time;
	flush_exec_min = qconf->flush_job.min_exec_time;
	flush_exec_max = qconf->flush_job.max_exec_time;
	rte_jobstats_reset(&qconf->flush_job);

	idle_exec_cnt = qconf->idle_job.exec_cnt;
	idle_exec = qconf->idle_job.exec_time;
	idle_exec_min = qconf->idle_job.min_exec_time;
	idle_exec_max = qconf->idle_job.max_exec_time;
	rte_jobstats_reset(&qconf->idle_job);

	rte_spinlock_unlock(&qconf->lock);

	exec -= idle_exec;
	busy = exec + management;
	busy_min = exec_min + management_min;
	busy_max = exec_max + management_max;


	collection_time = rte_get_timer_cycles() - collection_time;

#define STAT_FMT "\n%-18s %'14.0f %6.1f%% %'10.0f %'10.0f %'10.0f"

	printf("\n----------------"
			"\nLCore %3u: statistics (time in ns, collected in %'9.0f)"
			"\n%-18s %14s %7s %10s %10s %10s "
			"\n%-18s %'14.0f"
			"\n%-18s %'14" PRIu64
			STAT_FMT /* Exec */
			STAT_FMT /* Management */
			STAT_FMT /* Busy */
			STAT_FMT, /* Idle  */
			lcore_id, cycles_to_ns(collection_time),
			"Stat type", "total", "%total", "avg", "min", "max",
			"Stats duration:", cycles_to_ns(stats_period),
			"Loop count:", loop_count,
			"Exec time",
			cycles_to_ns(exec), exec * 100.0 / stats_period,
			cycles_to_ns(loop_count  ? exec / loop_count : 0),
			cycles_to_ns(exec_min),
			cycles_to_ns(exec_max),
			"Management time",
			cycles_to_ns(management), management * 100.0 / stats_period,
			cycles_to_ns(loop_count  ? management / loop_count : 0),
			cycles_to_ns(management_min),
			cycles_to_ns(management_max),
			"Exec + management",
			cycles_to_ns(busy),  busy * 100.0 / stats_period,
			cycles_to_ns(loop_count ? busy / loop_count : 0),
			cycles_to_ns(busy_min),
			cycles_to_ns(busy_max),
			"Idle (job)",
			cycles_to_ns(idle_exec), idle_exec * 100.0 / stats_period,
			cycles_to_ns(idle_exec_cnt ? idle_exec / idle_exec_cnt : 0),
			cycles_to_ns(idle_exec_min),
			cycles_to_ns(idle_exec_max));

	for (i = 0; i < qconf->n_rx_port; i++) {
		job = &qconf->port_fwd_jobs[i];
		printf("\n\nJob %" PRIu32 ": %-20s "
				"\n%-18s %'14" PRIu64
				"\n%-18s %'14.0f"
				STAT_FMT,
				i, job->name,
				"Exec count:", jobs_exec_cnt[i],
				"Exec period: ", cycles_to_ns(jobs_period[i]),
				"Exec time",
				cycles_to_ns(jobs_exec[i]), jobs_exec[i] * 100.0 / stats_period,
				cycles_to_ns(jobs_exec_cnt[i] ? jobs_exec[i] / jobs_exec_cnt[i]
						: 0),
				cycles_to_ns(jobs_exec_min[i]),
				cycles_to_ns(jobs_exec_max[i]));
	}

	if (qconf->n_rx_port > 0) {
		job = &qconf->flush_job;
		printf("\n\nJob %" PRIu32 ": %-20s "
				"\n%-18s %'14" PRIu64
				"\n%-18s %'14.0f"
				STAT_FMT,
				i, job->name,
				"Exec count:", flush_exec_cnt,
				"Exec period: ", cycles_to_ns(flush_period),
				"Exec time",
				cycles_to_ns(flush_exec), flush_exec * 100.0 / stats_period,
				cycles_to_ns(flush_exec_cnt ? flush_exec / flush_exec_cnt : 0),
				cycles_to_ns(flush_exec_min),
				cycles_to_ns(flush_exec_max));
	}
}

/* Print out statistics on packets dropped */
static void
show_stats_cb(__rte_unused void *param)
{
	uint64_t total_packets_dropped, total_packets_tx, total_packets_rx;
	unsigned portid, lcore_id;

	total_packets_dropped = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

	/* Clear screen and move to top left */
	printf("%s%s"
			"\nPort statistics ===================================",
			clr, topLeft);

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		printf("\nStatistics for port %u ------------------------------"
				"\nPackets sent: %24"PRIu64
				"\nPackets received: %20"PRIu64
				"\nPackets dropped: %21"PRIu64,
				portid,
				port_statistics[portid].tx,
				port_statistics[portid].rx,
				port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += port_statistics[portid].rx;
	}

	printf("\nAggregate statistics ==============================="
			"\nTotal packets sent: %18"PRIu64
			"\nTotal packets received: %14"PRIu64
			"\nTotal packets dropped: %15"PRIu64
			"\n====================================================",
			total_packets_tx,
			total_packets_rx,
			total_packets_dropped);

	RTE_LCORE_FOREACH(lcore_id) {
		if (lcore_queue_conf[lcore_id].n_rx_port > 0)
			show_lcore_stats(lcore_id);
	}

	printf("\n====================================================\n");
	rte_eal_alarm_set(timer_period * US_PER_S, show_stats_cb, NULL);
}

/* Send the burst of packets on an output interface */
static void
l2fwd_send_burst(struct lcore_queue_conf *qconf, uint8_t port)
{
	struct mbuf_table *m_table;
	uint16_t ret;
	uint16_t queueid = 0;
	uint16_t n;

	m_table = &qconf->tx_mbufs[port];
	n = m_table->len;

	m_table->next_flush_time = rte_get_timer_cycles() + drain_tsc;
	m_table->len = 0;

	ret = rte_eth_tx_burst(port, queueid, m_table->mbufs, n);

	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(m_table->mbufs[ret]);
		} while (++ret < n);
	}
}

/* Enqueue packets for TX and prepare them to be sent */
static int
l2fwd_send_packet(struct rte_mbuf *m, uint8_t port)
{
	const unsigned lcore_id = rte_lcore_id();
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	struct mbuf_table *m_table = &qconf->tx_mbufs[port];
	uint16_t len = qconf->tx_mbufs[port].len;

	m_table->mbufs[len] = m;

	len++;
	m_table->len = len;

	/* Enough pkts to be sent. */
	if (unlikely(len == MAX_PKT_BURST))
		l2fwd_send_burst(qconf, port);

	return 0;
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
{
	struct ether_hdr *eth;
	void *tmp;
	unsigned dst_port;

	dst_port = l2fwd_dst_ports[portid];
	eth = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

	l2fwd_send_packet(m, (uint8_t) dst_port);
}

static void
l2fwd_job_update_cb(struct rte_jobstats *job, int64_t result)
{
	int64_t err = job->target - result;
	int64_t histeresis = job->target / 8;

	if (err < -histeresis) {
		if (job->min_period + UPDATE_STEP_DOWN < job->period)
			job->period -= UPDATE_STEP_DOWN;
	} else if (err > histeresis) {
		if (job->period + UPDATE_STEP_UP < job->max_period)
			job->period += UPDATE_STEP_UP;
	}
}

static void
l2fwd_fwd_job(__rte_unused struct rte_timer *timer, void *arg)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m;

	const uint8_t port_idx = (uintptr_t) arg;
	const unsigned lcore_id = rte_lcore_id();
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	struct rte_jobstats *job = &qconf->port_fwd_jobs[port_idx];
	const uint8_t portid = qconf->rx_port_list[port_idx];

	uint8_t j;
	uint16_t total_nb_rx;

	rte_jobstats_start(&qconf->jobs_context, job);

	/* Call rx burst 2 times. This allow rte_jobstats logic to see if this
	 * function must be called more frequently. */

	total_nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst,
			MAX_PKT_BURST);

	for (j = 0; j < total_nb_rx; j++) {
		m = pkts_burst[j];
		rte_prefetch0(rte_pktmbuf_mtod(m, void *));
		l2fwd_simple_forward(m, portid);
	}

	if (total_nb_rx == MAX_PKT_BURST) {
		const uint16_t nb_rx = rte_eth_rx_burst((uint8_t) portid, 0, pkts_burst,
				MAX_PKT_BURST);

		total_nb_rx += nb_rx;
		for (j = 0; j < nb_rx; j++) {
			m = pkts_burst[j];
			rte_prefetch0(rte_pktmbuf_mtod(m, void *));
			l2fwd_simple_forward(m, portid);
		}
	}

	port_statistics[portid].rx += total_nb_rx;

	/* Adjust period time in which we are running here. */
	if (rte_jobstats_finish(job, total_nb_rx) != 0) {
		rte_timer_reset(&qconf->rx_timers[port_idx], job->period, PERIODICAL,
				lcore_id, l2fwd_fwd_job, arg);
	}
}

static void
l2fwd_flush_job(__rte_unused struct rte_timer *timer, __rte_unused void *arg)
{
	uint64_t now;
	unsigned lcore_id;
	struct lcore_queue_conf *qconf;
	struct mbuf_table *m_table;
	uint8_t portid;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	rte_jobstats_start(&qconf->jobs_context, &qconf->flush_job);

	now = rte_get_timer_cycles();
	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		m_table = &qconf->tx_mbufs[portid];
		if (m_table->len == 0 || m_table->next_flush_time <= now)
			continue;

		l2fwd_send_burst(qconf, portid);
	}


	/* Pass target to indicate that this job is happy of time interwal
	 * in which it was called. */
	rte_jobstats_finish(&qconf->flush_job, qconf->flush_job.target);
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	unsigned lcore_id;
	unsigned i, portid;
	struct lcore_queue_conf *qconf;
	uint8_t stats_read_pending = 0;
	uint8_t need_manage;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "lcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	rte_jobstats_init(&qconf->idle_job, "idle", 0, 0, 0, 0);

	for (;;) {
		rte_spinlock_lock(&qconf->lock);

		do {
			rte_jobstats_context_start(&qconf->jobs_context);

			/* Do the Idle job:
			 * - Read stats_read_pending flag
			 * - check if some real job need to be executed
			 */
			rte_jobstats_start(&qconf->jobs_context, &qconf->idle_job);

			do {
				uint8_t i;
				uint64_t now = rte_get_timer_cycles();

				need_manage = qconf->flush_timer.expire < now;
				/* Check if we was esked to give a stats. */
				stats_read_pending =
						rte_atomic16_read(&qconf->stats_read_pending);
				need_manage |= stats_read_pending;

				for (i = 0; i < qconf->n_rx_port && !need_manage; i++)
					need_manage = qconf->rx_timers[i].expire < now;

			} while (!need_manage);
			rte_jobstats_finish(&qconf->idle_job, qconf->idle_job.target);

			rte_timer_manage();
			rte_jobstats_context_finish(&qconf->jobs_context);
		} while (likely(stats_read_pending == 0));

		rte_spinlock_unlock(&qconf->lock);
		rte_pause();
	}
}

static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	l2fwd_main_loop();
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n"
		   "  -l set system default locale instead of default (\"C\" locale) for thousands separator in stats.",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
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
l2fwd_parse_timer_period(const char *q_arg)
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
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:l",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_parse_timer_period(optarg);
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* For thousands separator in printf. */
		case 'l':
			setlocale(LC_ALL, "");
			break;

		/* long options */
		case 0:
			l2fwd_usage(prgname);
			return -1;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	int ret;
	char name[RTE_JOBSTATS_NAMESIZE];
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	uint8_t i;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	rte_timer_subsystem_init();

	/* fetch default timer frequency. */
	hz = rte_get_timer_hz();

	/* create the mbuf pool */
	l2fwd_pktmbuf_pool =
		rte_pktmbuf_pool_create("mbuf_pool", NB_MBUF, 32,
			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (l2fwd_pktmbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		} else
			last_port = portid;

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       lcore_queue_conf[rx_lcore_id].n_rx_port ==
		       l2fwd_rx_queue_per_lcore) {
			rx_lcore_id++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
		}

		if (qconf != &lcore_queue_conf[rx_lcore_id])
			/* Assigned a new logical core in the loop above. */
			qconf = &lcore_queue_conf[rx_lcore_id];

		qconf->rx_port_list[qconf->n_rx_port] = portid;
		qconf->n_rx_port++;
		printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, 1, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid, &l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done:\n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	drain_tsc = (hz + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	RTE_LCORE_FOREACH(lcore_id) {
		qconf = &lcore_queue_conf[lcore_id];

		rte_spinlock_init(&qconf->lock);

		if (rte_jobstats_context_init(&qconf->jobs_context) != 0)
			rte_panic("Jobs stats context for core %u init failed\n", lcore_id);

		if (qconf->n_rx_port == 0) {
			RTE_LOG(INFO, L2FWD,
				"lcore %u: no ports so no jobs stats context initialization\n",
				lcore_id);
			continue;
		}
		/* Add flush job.
		 * Set fixed period by setting min = max = initial period. Set target to
		 * zero as it is irrelevant for this job. */
		rte_jobstats_init(&qconf->flush_job, "flush", drain_tsc, drain_tsc,
				drain_tsc, 0);

		rte_timer_init(&qconf->flush_timer);
		ret = rte_timer_reset(&qconf->flush_timer, drain_tsc, PERIODICAL,
				lcore_id, &l2fwd_flush_job, NULL);

		if (ret < 0) {
			rte_exit(1, "Failed to reset flush job timer for lcore %u: %s",
					lcore_id, rte_strerror(-ret));
		}

		for (i = 0; i < qconf->n_rx_port; i++) {
			struct rte_jobstats *job = &qconf->port_fwd_jobs[i];

			portid = qconf->rx_port_list[i];
			printf("Setting forward jon for port %u\n", portid);

			snprintf(name, RTE_DIM(name), "port %u fwd", portid);
			/* Setup forward job.
			 * Set min, max and initial period. Set target to MAX_PKT_BURST as
			 * this is desired optimal RX/TX burst size. */
			rte_jobstats_init(job, name, 0, drain_tsc, 0, MAX_PKT_BURST);
			rte_jobstats_set_update_period_function(job, l2fwd_job_update_cb);

			rte_timer_init(&qconf->rx_timers[i]);
			ret = rte_timer_reset(&qconf->rx_timers[i], 0, PERIODICAL, lcore_id,
					&l2fwd_fwd_job, (void *)(uintptr_t)i);

			if (ret < 0) {
				rte_exit(1, "Failed to reset lcore %u port %u job timer: %s",
						lcore_id, qconf->rx_port_list[i], rte_strerror(-ret));
			}
		}
	}

	if (timer_period)
		rte_eal_alarm_set(timer_period * MS_PER_S, show_stats_cb, NULL);
	else
		RTE_LOG(INFO, L2FWD, "Stats display disabled\n");

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
