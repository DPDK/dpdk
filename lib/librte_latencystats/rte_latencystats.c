/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <math.h>

#include <rte_mbuf.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_metrics.h>
#include <rte_memzone.h>
#include <rte_lcore.h>

#include "rte_latencystats.h"

/** Nano seconds per second */
#define NS_PER_SEC 1E9

/** Clock cycles per nano second */
static uint64_t
latencystat_cycles_per_ns(void)
{
	return rte_get_timer_hz() / NS_PER_SEC;
}

/* Macros for printing using RTE_LOG */
#define RTE_LOGTYPE_LATENCY_STATS RTE_LOGTYPE_USER1

static const char *MZ_RTE_LATENCY_STATS = "rte_latencystats";
static int latency_stats_index;
static uint64_t samp_intvl;
static uint64_t timer_tsc;
static uint64_t prev_tsc;

struct rte_latency_stats {
	float min_latency; /**< Minimum latency in nano seconds */
	float avg_latency; /**< Average latency in nano seconds */
	float max_latency; /**< Maximum latency in nano seconds */
	float jitter; /** Latency variation */
};

static struct rte_latency_stats *glob_stats;

struct rxtx_cbs {
	const struct rte_eth_rxtx_callback *cb;
};

static struct rxtx_cbs rx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];
static struct rxtx_cbs tx_cbs[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];

struct latency_stats_nameoff {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct latency_stats_nameoff lat_stats_strings[] = {
	{"min_latency_ns", offsetof(struct rte_latency_stats, min_latency)},
	{"avg_latency_ns", offsetof(struct rte_latency_stats, avg_latency)},
	{"max_latency_ns", offsetof(struct rte_latency_stats, max_latency)},
	{"jitter_ns", offsetof(struct rte_latency_stats, jitter)},
};

#define NUM_LATENCY_STATS (sizeof(lat_stats_strings) / \
				sizeof(lat_stats_strings[0]))

int32_t
rte_latencystats_update(void)
{
	unsigned int i;
	float *stats_ptr = NULL;
	uint64_t values[NUM_LATENCY_STATS] = {0};
	int ret;

	for (i = 0; i < NUM_LATENCY_STATS; i++) {
		stats_ptr = RTE_PTR_ADD(glob_stats,
				lat_stats_strings[i].offset);
		values[i] = (uint64_t)floor((*stats_ptr)/
				latencystat_cycles_per_ns());
	}

	ret = rte_metrics_update_values(RTE_METRICS_GLOBAL,
					latency_stats_index,
					values, NUM_LATENCY_STATS);
	if (ret < 0)
		RTE_LOG(INFO, LATENCY_STATS, "Failed to push the stats\n");

	return ret;
}

static void
rte_latencystats_fill_values(struct rte_metric_value *values)
{
	unsigned int i;
	float *stats_ptr = NULL;

	for (i = 0; i < NUM_LATENCY_STATS; i++) {
		stats_ptr = RTE_PTR_ADD(glob_stats,
				lat_stats_strings[i].offset);
		values[i].key = i;
		values[i].value = (uint64_t)floor((*stats_ptr)/
						latencystat_cycles_per_ns());
	}
}

static uint16_t
add_time_stamps(uint16_t pid __rte_unused,
		uint16_t qid __rte_unused,
		struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		uint16_t max_pkts __rte_unused,
		void *user_cb __rte_unused)
{
	unsigned int i;
	uint64_t diff_tsc, now;

	/*
	 * For every sample interval,
	 * time stamp is marked on one received packet.
	 */
	now = rte_rdtsc();
	for (i = 0; i < nb_pkts; i++) {
		diff_tsc = now - prev_tsc;
		timer_tsc += diff_tsc;
		if (timer_tsc >= samp_intvl) {
			pkts[i]->timestamp = now;
			timer_tsc = 0;
		}
		prev_tsc = now;
		now = rte_rdtsc();
	}

	return nb_pkts;
}

static uint16_t
calc_latency(uint16_t pid __rte_unused,
		uint16_t qid __rte_unused,
		struct rte_mbuf **pkts,
		uint16_t nb_pkts,
		void *_ __rte_unused)
{
	unsigned int i, cnt = 0;
	uint64_t now;
	float latency[nb_pkts];
	static float prev_latency;
	/*
	 * Alpha represents degree of weighting decrease in EWMA,
	 * a constant smoothing factor between 0 and 1. The value
	 * is used below for measuring average latency.
	 */
	const float alpha = 0.2;

	now = rte_rdtsc();
	for (i = 0; i < nb_pkts; i++) {
		if (pkts[i]->timestamp)
			latency[cnt++] = now - pkts[i]->timestamp;
	}

	for (i = 0; i < cnt; i++) {
		/*
		 * The jitter is calculated as statistical mean of interpacket
		 * delay variation. The "jitter estimate" is computed by taking
		 * the absolute values of the ipdv sequence and applying an
		 * exponential filter with parameter 1/16 to generate the
		 * estimate. i.e J=J+(|D(i-1,i)|-J)/16. Where J is jitter,
		 * D(i-1,i) is difference in latency of two consecutive packets
		 * i-1 and i.
		 * Reference: Calculated as per RFC 5481, sec 4.1,
		 * RFC 3393 sec 4.5, RFC 1889 sec.
		 */
		glob_stats->jitter +=  (fabsf(prev_latency - latency[i])
					- glob_stats->jitter)/16;
		if (glob_stats->min_latency == 0)
			glob_stats->min_latency = latency[i];
		else if (latency[i] < glob_stats->min_latency)
			glob_stats->min_latency = latency[i];
		else if (latency[i] > glob_stats->max_latency)
			glob_stats->max_latency = latency[i];
		/*
		 * The average latency is measured using exponential moving
		 * average, i.e. using EWMA
		 * https://en.wikipedia.org/wiki/Moving_average
		 */
		glob_stats->avg_latency +=
			alpha * (latency[i] - glob_stats->avg_latency);
		prev_latency = latency[i];
	}

	return nb_pkts;
}

int
rte_latencystats_init(uint64_t app_samp_intvl,
		rte_latency_stats_flow_type_fn user_cb)
{
	unsigned int i;
	uint16_t pid;
	uint16_t qid;
	struct rxtx_cbs *cbs = NULL;
	const char *ptr_strings[NUM_LATENCY_STATS] = {0};
	const struct rte_memzone *mz = NULL;
	const unsigned int flags = 0;

	if (rte_memzone_lookup(MZ_RTE_LATENCY_STATS))
		return -EEXIST;

	/** Allocate stats in shared memory fo multi process support */
	mz = rte_memzone_reserve(MZ_RTE_LATENCY_STATS, sizeof(*glob_stats),
					rte_socket_id(), flags);
	if (mz == NULL) {
		RTE_LOG(ERR, LATENCY_STATS, "Cannot reserve memory: %s:%d\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	glob_stats = mz->addr;
	samp_intvl = app_samp_intvl * latencystat_cycles_per_ns();

	/** Register latency stats with stats library */
	for (i = 0; i < NUM_LATENCY_STATS; i++)
		ptr_strings[i] = lat_stats_strings[i].name;

	latency_stats_index = rte_metrics_reg_names(ptr_strings,
							NUM_LATENCY_STATS);
	if (latency_stats_index < 0) {
		RTE_LOG(DEBUG, LATENCY_STATS,
			"Failed to register latency stats names\n");
		return -1;
	}

	/** Register Rx/Tx callbacks */
	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;
		rte_eth_dev_info_get(pid, &dev_info);
		for (qid = 0; qid < dev_info.nb_rx_queues; qid++) {
			cbs = &rx_cbs[pid][qid];
			cbs->cb = rte_eth_add_first_rx_callback(pid, qid,
					add_time_stamps, user_cb);
			if (!cbs->cb)
				RTE_LOG(INFO, LATENCY_STATS, "Failed to "
					"register Rx callback for pid=%d, "
					"qid=%d\n", pid, qid);
		}
		for (qid = 0; qid < dev_info.nb_tx_queues; qid++) {
			cbs = &tx_cbs[pid][qid];
			cbs->cb =  rte_eth_add_tx_callback(pid, qid,
					calc_latency, user_cb);
			if (!cbs->cb)
				RTE_LOG(INFO, LATENCY_STATS, "Failed to "
					"register Tx callback for pid=%d, "
					"qid=%d\n", pid, qid);
		}
	}
	return 0;
}

int
rte_latencystats_uninit(void)
{
	uint16_t pid;
	uint16_t qid;
	int ret = 0;
	struct rxtx_cbs *cbs = NULL;

	/** De register Rx/Tx callbacks */
	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;
		rte_eth_dev_info_get(pid, &dev_info);
		for (qid = 0; qid < dev_info.nb_rx_queues; qid++) {
			cbs = &rx_cbs[pid][qid];
			ret = rte_eth_remove_rx_callback(pid, qid, cbs->cb);
			if (ret)
				RTE_LOG(INFO, LATENCY_STATS, "failed to "
					"remove Rx callback for pid=%d, "
					"qid=%d\n", pid, qid);
		}
		for (qid = 0; qid < dev_info.nb_tx_queues; qid++) {
			cbs = &tx_cbs[pid][qid];
			ret = rte_eth_remove_tx_callback(pid, qid, cbs->cb);
			if (ret)
				RTE_LOG(INFO, LATENCY_STATS, "failed to "
					"remove Tx callback for pid=%d, "
					"qid=%d\n", pid, qid);
		}
	}

	return 0;
}

int
rte_latencystats_get_names(struct rte_metric_name *names, uint16_t size)
{
	unsigned int i;

	if (names == NULL || size < NUM_LATENCY_STATS)
		return NUM_LATENCY_STATS;

	for (i = 0; i < NUM_LATENCY_STATS; i++)
		snprintf(names[i].name, sizeof(names[i].name),
				"%s", lat_stats_strings[i].name);

	return NUM_LATENCY_STATS;
}

int
rte_latencystats_get(struct rte_metric_value *values, uint16_t size)
{
	if (size < NUM_LATENCY_STATS || values == NULL)
		return NUM_LATENCY_STATS;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		const struct rte_memzone *mz;
		mz = rte_memzone_lookup(MZ_RTE_LATENCY_STATS);
		if (mz == NULL) {
			RTE_LOG(ERR, LATENCY_STATS,
				"Latency stats memzone not found\n");
			return -ENOMEM;
		}
		glob_stats =  mz->addr;
	}

	/* Retrieve latency stats */
	rte_latencystats_fill_values(values);

	return NUM_LATENCY_STATS;
}
