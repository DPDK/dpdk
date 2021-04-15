/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>

#include <rte_errno.h>
#include <rte_jobstats.h>
#include <rte_timer.h>
#include <rte_alarm.h>
#include <rte_pause.h>

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

#define UPDATE_STEP_UP 1
#define UPDATE_STEP_DOWN 32

static unsigned int l2fwd_rx_queue_per_lcore = 1;

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	uint64_t next_flush_time[RTE_MAX_ETHPORTS];

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

struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
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
	const uint16_t port_cnt = qconf->n_rx_port;
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

	fflush(stdout);

	rte_eal_alarm_set(timer_period * US_PER_S, show_stats_cb, NULL);
}

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid)
{
	struct rte_ether_hdr *eth;
	void *tmp;
	int sent;
	unsigned dst_port;
	struct rte_eth_dev_tx_buffer *buffer;

	dst_port = l2fwd_dst_ports[portid];
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	rte_ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

	buffer = tx_buffer[dst_port];
	sent = rte_eth_tx_buffer(dst_port, 0, buffer, m);
	if (sent)
		port_statistics[dst_port].tx += sent;
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

	const uint16_t port_idx = (uintptr_t) arg;
	const unsigned lcore_id = rte_lcore_id();
	struct lcore_queue_conf *qconf = &lcore_queue_conf[lcore_id];
	struct rte_jobstats *job = &qconf->port_fwd_jobs[port_idx];
	const uint16_t portid = qconf->rx_port_list[port_idx];

	uint8_t j;
	uint16_t total_nb_rx;

	rte_jobstats_start(&qconf->jobs_context, job);

	/* Call rx burst 2 times. This allow rte_jobstats logic to see if this
	 * function must be called more frequently. */

	total_nb_rx = rte_eth_rx_burst(portid, 0, pkts_burst,
			MAX_PKT_BURST);

	for (j = 0; j < total_nb_rx; j++) {
		m = pkts_burst[j];
		rte_prefetch0(rte_pktmbuf_mtod(m, void *));
		l2fwd_simple_forward(m, portid);
	}

	if (total_nb_rx == MAX_PKT_BURST) {
		const uint16_t nb_rx = rte_eth_rx_burst(portid, 0, pkts_burst,
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
	uint16_t portid;
	unsigned i;
	uint32_t sent;
	struct rte_eth_dev_tx_buffer *buffer;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	rte_jobstats_start(&qconf->jobs_context, &qconf->flush_job);

	now = rte_get_timer_cycles();
	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	for (i = 0; i < qconf->n_rx_port; i++) {
		portid = l2fwd_dst_ports[qconf->rx_port_list[i]];

		if (qconf->next_flush_time[portid] <= now)
			continue;

		buffer = tx_buffer[portid];
		sent = rte_eth_tx_buffer_flush(portid, 0, buffer);
		if (sent)
			port_statistics[portid].tx += sent;

		qconf->next_flush_time[portid] = rte_get_timer_cycles() + drain_tsc;
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

			uint64_t repeats = 0;

			do {
				uint8_t i;
				uint64_t now = rte_get_timer_cycles();

				repeats++;
				need_manage = qconf->flush_timer.expire < now;
				/* Check if we was esked to give a stats. */
				stats_read_pending =
						rte_atomic16_read(&qconf->stats_read_pending);
				need_manage |= stats_read_pending;

				for (i = 0; i < qconf->n_rx_port && !need_manage; i++)
					need_manage = qconf->rx_timers[i].expire < now;

			} while (!need_manage);

			if (likely(repeats != 1))
				rte_jobstats_finish(&qconf->idle_job, qconf->idle_job.target);
			else
				rte_jobstats_abort(&qconf->idle_job);

			rte_timer_manage();
			rte_jobstats_context_finish(&qconf->jobs_context);
		} while (likely(stats_read_pending == 0));

		rte_spinlock_unlock(&qconf->lock);
		rte_pause();
	}
}

static int
l2fwd_launch_one_lcore(__rte_unused void *dummy)
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
		return 0;

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
	optind = 1; /* reset getopt lib */
	return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
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
	unsigned lcore_id, rx_lcore_id;
	unsigned nb_ports_in_mask = 0;
	int ret;
	char name[RTE_JOBSTATS_NAMESIZE];
	uint16_t nb_ports;
	uint16_t nb_ports_available = 0;
	uint16_t portid, last_port;
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

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	RTE_ETH_FOREACH_DEV(portid) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		} else
			last_port = portid;

		nb_ports_in_mask++;
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	RTE_ETH_FOREACH_DEV(portid) {
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
		printf("Lcore %u: RX port %u\n", rx_lcore_id, portid);
	}

	/* Initialise each port */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_dev_info dev_info;
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_conf local_port_conf = port_conf;

		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", portid);
			continue;
		}
		nb_ports_available++;

		/* init port */
		printf("Initializing port %u... ", portid);
		fflush(stdout);

		ret = rte_eth_dev_info_get(portid, &dev_info);
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				"Error during getting device (port %u) info: %s\n",
				portid, strerror(-ret));

		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, portid);

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
						       &nb_txd);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot adjust number of descriptors: err=%d, port=%u\n",
				 ret, portid);

		ret = rte_eth_macaddr_get(portid,
					  &l2fwd_ports_eth_addr[portid]);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
				 "Cannot get MAC address: err=%d, port=%u\n",
				 ret, portid);

		/* init one RX queue */
		fflush(stdout);
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     &rxq_conf,
					     l2fwd_pktmbuf_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n",
				  ret, portid);

		/* init one TX queue on each port */
		txq_conf = dev_info.default_txconf;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				&txq_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, portid);

		/* Initialize TX buffers */
		tx_buffer[portid] = rte_zmalloc_socket("tx_buffer",
				RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
				rte_eth_dev_socket_id(portid));
		if (tx_buffer[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
					portid);

		rte_eth_tx_buffer_init(tx_buffer[portid], MAX_PKT_BURST);

		ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[portid],
				rte_eth_tx_buffer_count_callback,
				&port_statistics[portid].dropped);
		if (ret < 0)
			rte_exit(EXIT_FAILURE,
			"Cannot set error callback for tx buffer on port %u\n",
				 portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, portid);

		printf("done:\n");

		ret = rte_eth_promiscuous_enable(portid);
		if (ret != 0) {
			rte_exit(EXIT_FAILURE,
				 "rte_eth_promiscuous_enable:err=%s, port=%u\n",
				 rte_strerror(-ret), portid);
			return ret;

		}

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				portid,
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

	check_all_ports_link_status(l2fwd_enabled_port_mask);

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
			printf("Setting forward job for port %u\n", portid);

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
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MAIN);
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
