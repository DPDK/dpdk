/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <eal_export.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <rte_memzone.h>
#include <rte_metrics.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <rte_stdatomic.h>

#include "rte_latencystats.h"

/** Nano seconds per second */
#define NS_PER_SEC 1E9

static double cycles_per_ns;

static RTE_LOG_REGISTER_DEFAULT(latencystat_logtype, INFO);
#define RTE_LOGTYPE_LATENCY_STATS latencystat_logtype
#define LATENCY_STATS_LOG(level, ...) \
	RTE_LOG_LINE(level, LATENCY_STATS, "" __VA_ARGS__)

static uint64_t timestamp_dynflag;
static int timestamp_dynfield_offset = -1;

static inline rte_mbuf_timestamp_t *
timestamp_dynfield(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
			timestamp_dynfield_offset, rte_mbuf_timestamp_t *);
}

/* Compare two 64 bit timer counter but deal with wraparound correctly. */
static inline bool tsc_after(uint64_t t0, uint64_t t1)
{
	return (int64_t)(t1 - t0) < 0;
}

#define tsc_before(a, b) tsc_after(b, a)

static const char *MZ_RTE_LATENCY_STATS = "rte_latencystats";
static int latency_stats_index;

static rte_spinlock_t sample_lock = RTE_SPINLOCK_INITIALIZER;
static uint64_t samp_intvl;
static RTE_ATOMIC(uint64_t) next_tsc;

#define LATENCY_AVG_SCALE     4
#define LATENCY_JITTER_SCALE 16

struct rte_latency_stats {
	uint64_t min_latency; /**< Minimum latency */
	uint64_t avg_latency; /**< Average latency */
	uint64_t max_latency; /**< Maximum latency */
	uint64_t jitter; /** Latency variation */
	uint64_t samples;    /** Number of latency samples */
	rte_spinlock_t lock; /** Latency calculation lock */
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
	unsigned int scale;
};

static const struct latency_stats_nameoff lat_stats_strings[] = {
	{"min_latency_ns", offsetof(struct rte_latency_stats, min_latency), 1},
	{"avg_latency_ns", offsetof(struct rte_latency_stats, avg_latency), LATENCY_AVG_SCALE},
	{"max_latency_ns", offsetof(struct rte_latency_stats, max_latency), 1},
	{"jitter_ns", offsetof(struct rte_latency_stats, jitter), LATENCY_JITTER_SCALE},
	{"samples", offsetof(struct rte_latency_stats, samples), 0},
};

#define NUM_LATENCY_STATS RTE_DIM(lat_stats_strings)

static void
latencystats_collect(uint64_t values[])
{
	unsigned int i, scale;
	const uint64_t *stats;

	for (i = 0; i < NUM_LATENCY_STATS; i++) {
		stats = RTE_PTR_ADD(glob_stats, lat_stats_strings[i].offset);
		scale = lat_stats_strings[i].scale;

		/* used to mark samples which are not a time interval */
		if (scale == 0)
			values[i] = *stats;
		else
			values[i] = floor(*stats / (cycles_per_ns * scale));
	}
}

RTE_EXPORT_SYMBOL(rte_latencystats_update)
int32_t
rte_latencystats_update(void)
{
	uint64_t values[NUM_LATENCY_STATS];
	int ret;

	latencystats_collect(values);

	ret = rte_metrics_update_values(RTE_METRICS_GLOBAL,
					latency_stats_index,
					values, NUM_LATENCY_STATS);
	if (ret < 0)
		LATENCY_STATS_LOG(INFO, "Failed to push the stats");

	return ret;
}

static void
rte_latencystats_fill_values(struct rte_metric_value *metrics)
{
	uint64_t values[NUM_LATENCY_STATS];
	unsigned int i;

	latencystats_collect(values);

	for (i = 0; i < NUM_LATENCY_STATS; i++) {
		metrics[i].key = i;
		metrics[i].value = values[i];
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
	uint64_t now = rte_rdtsc();

	/* Check without locking */
	if (likely(tsc_before(now, rte_atomic_load_explicit(&next_tsc,
							    rte_memory_order_relaxed))))
		return nb_pkts;

	/* Try and get sample, skip if sample is being done by other core. */
	if (likely(rte_spinlock_trylock(&sample_lock))) {
		for (i = 0; i < nb_pkts; i++) {
			struct rte_mbuf *m = pkts[i];

			/* skip if already timestamped */
			if (unlikely(m->ol_flags & timestamp_dynflag))
				continue;

			m->ol_flags |= timestamp_dynflag;
			*timestamp_dynfield(m) = now;
			rte_atomic_store_explicit(&next_tsc, now + samp_intvl,
						  rte_memory_order_relaxed);
			break;
		}
		rte_spinlock_unlock(&sample_lock);
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
	unsigned int i;
	uint64_t now, latency;
	uint64_t ts_flags = 0;
	static uint64_t prev_latency;

	for (i = 0; i < nb_pkts; i++)
		ts_flags |= (pkts[i]->ol_flags & timestamp_dynflag);

	/* no samples in this burst, skip locking */
	if (likely(ts_flags == 0))
		return nb_pkts;

	now = rte_rdtsc();
	rte_spinlock_lock(&glob_stats->lock);
	for (i = 0; i < nb_pkts; i++) {
		if (!(pkts[i]->ol_flags & timestamp_dynflag))
			continue;

		latency = now - *timestamp_dynfield(pkts[i]);

		if (glob_stats->samples++ == 0) {
			glob_stats->min_latency = latency;
			glob_stats->max_latency = latency;
			glob_stats->avg_latency = latency * 4;
			/* start ad if previous sample had 0 latency */
			glob_stats->jitter = latency / LATENCY_JITTER_SCALE;
		} else {
			/*
			 * The jitter is calculated as statistical mean of interpacket
			 * delay variation. The "jitter estimate" is computed by taking
			 * the absolute values of the ipdv sequence and applying an
			 * exponential filter with parameter 1/16 to generate the
			 * estimate. i.e J=J+(|D(i-1,i)|-J)/16. Where J is jitter,
			 * D(i-1,i) is difference in latency of two consecutive packets
			 * i-1 and i. Jitter is scaled by 16.
			 * Reference: Calculated as per RFC 5481, sec 4.1,
			 * RFC 3393 sec 4.5, RFC 1889 sec.
			 */
			long long delta = prev_latency - latency;
			glob_stats->jitter += llabs(delta)
				- glob_stats->jitter / LATENCY_JITTER_SCALE;

			if (latency < glob_stats->min_latency)
				glob_stats->min_latency = latency;
			if (latency > glob_stats->max_latency)
				glob_stats->max_latency = latency;
			/*
			 * The average latency is measured using exponential moving
			 * average, i.e. using EWMA
			 * https://en.wikipedia.org/wiki/Moving_average
			 *
			 * Alpha is .25, avg_latency is scaled by 4.
			 */
			glob_stats->avg_latency += latency
				- glob_stats->avg_latency / LATENCY_AVG_SCALE;
		}

		prev_latency = latency;
	}
	rte_spinlock_unlock(&glob_stats->lock);

	return nb_pkts;
}

RTE_EXPORT_SYMBOL(rte_latencystats_init)
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
	int ret;

	if (rte_memzone_lookup(MZ_RTE_LATENCY_STATS))
		return -EEXIST;

	/** Reserved for possible future use */
	if (user_cb != NULL)
		return -ENOTSUP;

	/** Allocate stats in shared memory fo multi process support */
	mz = rte_memzone_reserve(MZ_RTE_LATENCY_STATS, sizeof(*glob_stats),
					rte_socket_id(), flags);
	if (mz == NULL) {
		LATENCY_STATS_LOG(ERR, "Cannot reserve memory: %s:%d",
			__func__, __LINE__);
		return -ENOMEM;
	}

	cycles_per_ns = (double)rte_get_tsc_hz() / NS_PER_SEC;

	glob_stats = mz->addr;
	rte_spinlock_init(&glob_stats->lock);
	samp_intvl = (uint64_t)(app_samp_intvl * cycles_per_ns);
	next_tsc = rte_rdtsc();

	/** Register latency stats with stats library */
	for (i = 0; i < NUM_LATENCY_STATS; i++)
		ptr_strings[i] = lat_stats_strings[i].name;

	latency_stats_index = rte_metrics_reg_names(ptr_strings,
							NUM_LATENCY_STATS);
	if (latency_stats_index < 0) {
		LATENCY_STATS_LOG(ERR,
			"Failed to register latency stats names");
		return -1;
	}

	/* Register mbuf field and flag for Rx timestamp */
	ret = rte_mbuf_dyn_rx_timestamp_register(&timestamp_dynfield_offset,
			&timestamp_dynflag);
	if (ret != 0) {
		LATENCY_STATS_LOG(ERR,
			"Cannot register mbuf field/flag for timestamp");
		return -rte_errno;
	}

	/** Register Rx/Tx callbacks */
	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;

		ret = rte_eth_dev_info_get(pid, &dev_info);
		if (ret != 0) {
			LATENCY_STATS_LOG(NOTICE,
				"Can not get info for device (port %u): %s",
				pid, strerror(-ret));

			continue;
		}

		for (qid = 0; qid < dev_info.nb_rx_queues; qid++) {
			cbs = &rx_cbs[pid][qid];
			cbs->cb = rte_eth_add_first_rx_callback(pid, qid,
					add_time_stamps, NULL);
			if (!cbs->cb)
				LATENCY_STATS_LOG(NOTICE,
					"Failed to register Rx callback for pid=%u, qid=%u",
					pid, qid);
		}
		for (qid = 0; qid < dev_info.nb_tx_queues; qid++) {
			cbs = &tx_cbs[pid][qid];
			cbs->cb =  rte_eth_add_tx_callback(pid, qid,
					calc_latency, NULL);
			if (!cbs->cb)
				LATENCY_STATS_LOG(NOTICE,
					"Failed to register Tx callback for pid=%u, qid=%u",
					pid, qid);
		}
	}
	return 0;
}

RTE_EXPORT_SYMBOL(rte_latencystats_uninit)
int
rte_latencystats_uninit(void)
{
	uint16_t pid;
	uint16_t qid;
	int ret = 0;
	struct rxtx_cbs *cbs = NULL;
	const struct rte_memzone *mz = NULL;

	/** De register Rx/Tx callbacks */
	RTE_ETH_FOREACH_DEV(pid) {
		struct rte_eth_dev_info dev_info;

		ret = rte_eth_dev_info_get(pid, &dev_info);
		if (ret != 0) {
			LATENCY_STATS_LOG(NOTICE,
				"Can not get info for device (port %u): %s",
				pid, strerror(-ret));
			continue;
		}

		for (qid = 0; qid < dev_info.nb_rx_queues; qid++) {
			cbs = &rx_cbs[pid][qid];
			ret = rte_eth_remove_rx_callback(pid, qid, cbs->cb);
			if (ret)
				LATENCY_STATS_LOG(NOTICE,
					"Failed to remove Rx callback for pid=%u, qid=%u",
					pid, qid);
		}
		for (qid = 0; qid < dev_info.nb_tx_queues; qid++) {
			cbs = &tx_cbs[pid][qid];
			ret = rte_eth_remove_tx_callback(pid, qid, cbs->cb);
			if (ret)
				LATENCY_STATS_LOG(NOTICE,
					"Failed to remove Tx callback for pid=%u, qid=%u",
					pid, qid);
		}
	}

	/* free up the memzone */
	mz = rte_memzone_lookup(MZ_RTE_LATENCY_STATS);
	rte_memzone_free(mz);

	return 0;
}

RTE_EXPORT_SYMBOL(rte_latencystats_get_names)
int
rte_latencystats_get_names(struct rte_metric_name *names, uint16_t size)
{
	unsigned int i;

	if (names == NULL || size < NUM_LATENCY_STATS)
		return NUM_LATENCY_STATS;

	for (i = 0; i < NUM_LATENCY_STATS; i++)
		strlcpy(names[i].name, lat_stats_strings[i].name,
			sizeof(names[i].name));

	return NUM_LATENCY_STATS;
}

RTE_EXPORT_SYMBOL(rte_latencystats_get)
int
rte_latencystats_get(struct rte_metric_value *values, uint16_t size)
{
	if (size < NUM_LATENCY_STATS || values == NULL)
		return NUM_LATENCY_STATS;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		const struct rte_memzone *mz;
		mz = rte_memzone_lookup(MZ_RTE_LATENCY_STATS);
		if (mz == NULL) {
			LATENCY_STATS_LOG(ERR,
				"Latency stats memzone not found");
			return -ENOMEM;
		}
		glob_stats =  mz->addr;
	}

	/* Retrieve latency stats */
	rte_latencystats_fill_values(values);

	return NUM_LATENCY_STATS;
}
