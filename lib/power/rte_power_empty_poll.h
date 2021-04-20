/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _RTE_EMPTY_POLL_H
#define _RTE_EMPTY_POLL_H

/**
 * @file
 * RTE Power Management
 */
#include <stdint.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_power.h>
#include <rte_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_FREQS  RTE_MAX_LCORE_FREQS

#define BINS_AV 4 /* Has to be ^2 */

#define DROP (NUM_DIRECTIONS * NUM_DEVICES)

#define NUM_PRIORITIES          2

#define NUM_NODES         256  /* Max core number*/

/* Processor Power State */
enum freq_val {
	LOW,
	MED,
	HGH,
	NUM_FREQ = NUM_FREQS
};


/* Queue Polling State */
enum queue_state {
	TRAINING, /* NO TRAFFIC */
	MED_NORMAL,   /* MED */
	HGH_BUSY,     /* HIGH */
	LOW_PURGE,    /* LOW */
};

/* Queue Stats */
struct freq_threshold {

	uint64_t base_edpi;
	bool trained;
	uint32_t threshold_percent;
	uint32_t cur_train_iter;
};

/* Each Worker Thread Empty Poll Stats */
struct priority_worker {

	/* Current dequeue and throughput counts */
	/* These 2 are written to by the worker threads */
	/* So keep them on their own cache line */
	uint64_t empty_dequeues;
	uint64_t num_dequeue_pkts;

	enum queue_state queue_state;

	uint64_t empty_dequeues_prev;
	uint64_t num_dequeue_pkts_prev;

	/* Used for training only */
	struct freq_threshold thresh[NUM_FREQ];
	enum freq_val cur_freq;

	/* bucket arrays to calculate the averages */
	/* edpi mean empty poll counter difference per interval */
	uint64_t edpi_av[BINS_AV];
	/* empty poll counter */
	uint32_t ec;
	/* ppi mean valid poll counter per interval */
	uint64_t ppi_av[BINS_AV];
	/* valid poll counter */
	uint32_t pc;

	uint32_t lcore_id;
	uint32_t iter_counter;
	uint32_t threshold_ctr;
	uint32_t display_ctr;
	uint8_t  dev_id;

} __rte_cache_aligned;


struct stats_data {

	struct priority_worker wrk_stats[NUM_NODES];

	/* flag to stop rx threads processing packets until training over */
	bool start_rx;

};

/* Empty Poll Parameters */
struct ep_params {

	/* Timer related stuff */
	uint64_t interval_ticks;
	uint32_t max_train_iter;

	struct rte_timer timer0;
	struct stats_data wrk_data;
};


/* Sample App Init information */
struct ep_policy {

	uint64_t med_base_edpi;
	uint64_t hgh_base_edpi;

	enum queue_state state;
};



/**
 * Initialize the power management system.
 *
 * @param eptr
 *   the structure of empty poll configuration
 * @param freq_tlb
 *   the power state/frequency mapping table
 * @param policy
 *   the initialization policy from sample app
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int
rte_power_empty_poll_stat_init(struct ep_params **eptr, uint8_t *freq_tlb,
		struct ep_policy *policy);

/**
 * Free the resource hold by power management system.
 */
__rte_experimental
void
rte_power_empty_poll_stat_free(void);

/**
 * Update specific core empty poll counter
 * It's not thread safe.
 *
 * @param lcore_id
 *  lcore id
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int
rte_power_empty_poll_stat_update(unsigned int lcore_id);

/**
 * Update specific core valid poll counter, not thread safe.
 *
 * @param lcore_id
 *  lcore id.
 * @param nb_pkt
 *  The packet number of one valid poll.
 *
 * @return
 *  - 0 on success.
 *  - Negative on error.
 */
__rte_experimental
int
rte_power_poll_stat_update(unsigned int lcore_id, uint8_t nb_pkt);

/**
 * Fetch specific core empty poll counter.
 *
 * @param lcore_id
 *  lcore id
 *
 * @return
 *  Current lcore empty poll counter value.
 */
__rte_experimental
uint64_t
rte_power_empty_poll_stat_fetch(unsigned int lcore_id);

/**
 * Fetch specific core valid poll counter.
 *
 * @param lcore_id
 *  lcore id
 *
 * @return
 *  Current lcore valid poll counter value.
 */
__rte_experimental
uint64_t
rte_power_poll_stat_fetch(unsigned int lcore_id);

/**
 * Empty poll  state change detection function
 *
 * @param  tim
 *  The timer structure
 * @param  arg
 *  The customized parameter
 */
__rte_experimental
void
rte_empty_poll_detection(struct rte_timer *tim, void *arg);

#ifdef __cplusplus
}
#endif

#endif
