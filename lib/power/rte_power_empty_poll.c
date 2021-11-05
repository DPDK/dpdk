/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <string.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <inttypes.h>

#include "rte_power.h"
#include "rte_power_empty_poll.h"

#define INTERVALS_PER_SECOND 100     /* (10ms) */
#define SECONDS_TO_TRAIN_FOR 2
#define DEFAULT_MED_TO_HIGH_PERCENT_THRESHOLD 70
#define DEFAULT_HIGH_TO_MED_PERCENT_THRESHOLD 30
#define DEFAULT_CYCLES_PER_PACKET 800

static struct ep_params *ep_params;
static uint32_t med_to_high_threshold = DEFAULT_MED_TO_HIGH_PERCENT_THRESHOLD;
static uint32_t high_to_med_threshold = DEFAULT_HIGH_TO_MED_PERCENT_THRESHOLD;

static uint32_t avail_freqs[RTE_MAX_LCORE][NUM_FREQS];

static uint32_t total_avail_freqs[RTE_MAX_LCORE];

static uint32_t freq_index[NUM_FREQ];

static uint32_t
get_freq_index(enum freq_val index)
{
	return freq_index[index];
}


static int
set_power_freq(int lcore_id, enum freq_val freq, bool specific_freq)
{
	int err = 0;
	uint32_t power_freq_index;
	if (!specific_freq)
		power_freq_index = get_freq_index(freq);
	else
		power_freq_index = freq;

	err = rte_power_set_freq(lcore_id, power_freq_index);

	return err;
}


static __rte_always_inline void
exit_training_state(struct priority_worker *poll_stats)
{
	RTE_SET_USED(poll_stats);
}

static __rte_always_inline void
enter_training_state(struct priority_worker *poll_stats)
{
	poll_stats->iter_counter = 0;
	poll_stats->cur_freq = LOW;
	poll_stats->queue_state = TRAINING;
}

static __rte_always_inline void
enter_normal_state(struct priority_worker *poll_stats)
{
	/* Clear the averages arrays and strs */
	memset(poll_stats->edpi_av, 0, sizeof(poll_stats->edpi_av));
	poll_stats->ec = 0;

	poll_stats->cur_freq = MED;
	poll_stats->iter_counter = 0;
	poll_stats->threshold_ctr = 0;
	poll_stats->queue_state = MED_NORMAL;
	RTE_LOG(INFO, POWER, "Set the power freq to MED\n");
	set_power_freq(poll_stats->lcore_id, MED, false);

	poll_stats->thresh[MED].threshold_percent = med_to_high_threshold;
	poll_stats->thresh[HGH].threshold_percent = high_to_med_threshold;
}

static __rte_always_inline void
enter_busy_state(struct priority_worker *poll_stats)
{
	memset(poll_stats->edpi_av, 0, sizeof(poll_stats->edpi_av));
	poll_stats->ec = 0;

	poll_stats->cur_freq = HGH;
	poll_stats->iter_counter = 0;
	poll_stats->threshold_ctr = 0;
	poll_stats->queue_state = HGH_BUSY;
	set_power_freq(poll_stats->lcore_id, HGH, false);
}

static __rte_always_inline void
enter_purge_state(struct priority_worker *poll_stats)
{
	poll_stats->iter_counter = 0;
	poll_stats->queue_state = LOW_PURGE;
}

static __rte_always_inline void
set_state(struct priority_worker *poll_stats,
		enum queue_state new_state)
{
	enum queue_state old_state = poll_stats->queue_state;
	if (old_state != new_state) {

		/* Call any old state exit functions */
		if (old_state == TRAINING)
			exit_training_state(poll_stats);

		/* Call any new state entry functions */
		if (new_state == TRAINING)
			enter_training_state(poll_stats);
		if (new_state == MED_NORMAL)
			enter_normal_state(poll_stats);
		if (new_state == HGH_BUSY)
			enter_busy_state(poll_stats);
		if (new_state == LOW_PURGE)
			enter_purge_state(poll_stats);
	}
}

static __rte_always_inline void
set_policy(struct priority_worker *poll_stats,
		struct ep_policy *policy)
{
	set_state(poll_stats, policy->state);

	if (policy->state == TRAINING)
		return;

	poll_stats->thresh[MED_NORMAL].base_edpi = policy->med_base_edpi;
	poll_stats->thresh[HGH_BUSY].base_edpi = policy->hgh_base_edpi;

	poll_stats->thresh[MED_NORMAL].trained = true;
	poll_stats->thresh[HGH_BUSY].trained = true;

}

static void
update_training_stats(struct priority_worker *poll_stats,
		uint32_t freq,
		bool specific_freq,
		uint32_t max_train_iter)
{
	RTE_SET_USED(specific_freq);

	uint64_t p0_empty_deq;

	if (poll_stats->cur_freq == freq &&
			poll_stats->thresh[freq].trained == false) {
		if (poll_stats->thresh[freq].cur_train_iter == 0) {

			set_power_freq(poll_stats->lcore_id,
					freq, specific_freq);

			poll_stats->empty_dequeues_prev =
				poll_stats->empty_dequeues;

			poll_stats->thresh[freq].cur_train_iter++;

			return;
		} else if (poll_stats->thresh[freq].cur_train_iter
				<= max_train_iter) {

			p0_empty_deq = poll_stats->empty_dequeues -
				poll_stats->empty_dequeues_prev;

			poll_stats->empty_dequeues_prev =
				poll_stats->empty_dequeues;

			poll_stats->thresh[freq].base_edpi += p0_empty_deq;
			poll_stats->thresh[freq].cur_train_iter++;

		} else {
			if (poll_stats->thresh[freq].trained == false) {
				poll_stats->thresh[freq].base_edpi =
					poll_stats->thresh[freq].base_edpi /
					max_train_iter;

				/* Add on a factor of 0.05%
				 * this should remove any
				 * false negatives when the system is 0% busy
				 */
				poll_stats->thresh[freq].base_edpi +=
				poll_stats->thresh[freq].base_edpi / 2000;

				poll_stats->thresh[freq].trained = true;
				poll_stats->cur_freq++;

			}
		}
	}
}

static __rte_always_inline uint32_t
update_stats(struct priority_worker *poll_stats)
{
	uint64_t tot_edpi = 0;
	uint32_t j, percent;

	struct priority_worker *s = poll_stats;

	uint64_t cur_edpi = s->empty_dequeues - s->empty_dequeues_prev;

	s->empty_dequeues_prev = s->empty_dequeues;

	if (s->thresh[s->cur_freq].base_edpi < cur_edpi) {

		/* edpi mean empty poll counter difference per interval */
		RTE_LOG(DEBUG, POWER, "cur_edpi is too large "
				"cur edpi %"PRId64" "
				"base edpi %"PRId64"\n",
				cur_edpi,
				s->thresh[s->cur_freq].base_edpi);
		/* Value to make us fail need debug log*/
		return 1000UL;
	}

	s->edpi_av[s->ec++ % BINS_AV] = cur_edpi;

	for (j = 0; j < BINS_AV; j++) {
		tot_edpi += s->edpi_av[j];
	}

	tot_edpi = tot_edpi / BINS_AV;

	percent = 100 - (uint32_t)(((float)tot_edpi /
			(float)s->thresh[s->cur_freq].base_edpi) * 100);

	return (uint32_t)percent;
}


static __rte_always_inline void
update_stats_normal(struct priority_worker *poll_stats)
{
	uint32_t percent;

	if (poll_stats->thresh[poll_stats->cur_freq].base_edpi == 0) {

		enum freq_val cur_freq = poll_stats->cur_freq;

		/* edpi mean empty poll counter difference per interval */
		RTE_LOG(DEBUG, POWER, "cure freq is %d, edpi is %"PRIu64"\n",
				cur_freq,
				poll_stats->thresh[cur_freq].base_edpi);
		return;
	}

	percent = update_stats(poll_stats);

	if (percent > 100) {
		/* edpi mean empty poll counter difference per interval */
		RTE_LOG(DEBUG, POWER, "Edpi is bigger than threshold\n");
		return;
	}

	if (poll_stats->cur_freq == LOW)
		RTE_LOG(INFO, POWER, "Purge Mode is not currently supported\n");
	else if (poll_stats->cur_freq == MED) {

		if (percent >
			poll_stats->thresh[MED].threshold_percent) {

			if (poll_stats->threshold_ctr < INTERVALS_PER_SECOND)
				poll_stats->threshold_ctr++;
			else {
				set_state(poll_stats, HGH_BUSY);
				RTE_LOG(INFO, POWER, "MOVE to HGH\n");
			}

		} else {
			/* reset */
			poll_stats->threshold_ctr = 0;
		}

	} else if (poll_stats->cur_freq == HGH) {

		if (percent <
				poll_stats->thresh[HGH].threshold_percent) {

			if (poll_stats->threshold_ctr < INTERVALS_PER_SECOND)
				poll_stats->threshold_ctr++;
			else {
				set_state(poll_stats, MED_NORMAL);
				RTE_LOG(INFO, POWER, "MOVE to MED\n");
			}
		} else {
			/* reset */
			poll_stats->threshold_ctr = 0;
		}

	}
}

static int
empty_poll_training(struct priority_worker *poll_stats,
		uint32_t max_train_iter)
{

	if (poll_stats->iter_counter < INTERVALS_PER_SECOND) {
		poll_stats->iter_counter++;
		return 0;
	}


	update_training_stats(poll_stats,
			LOW,
			false,
			max_train_iter);

	update_training_stats(poll_stats,
			MED,
			false,
			max_train_iter);

	update_training_stats(poll_stats,
			HGH,
			false,
			max_train_iter);


	if (poll_stats->thresh[LOW].trained == true
			&& poll_stats->thresh[MED].trained == true
			&& poll_stats->thresh[HGH].trained == true) {

		set_state(poll_stats, MED_NORMAL);

		RTE_LOG(INFO, POWER, "LOW threshold is %"PRIu64"\n",
				poll_stats->thresh[LOW].base_edpi);

		RTE_LOG(INFO, POWER, "MED threshold is %"PRIu64"\n",
				poll_stats->thresh[MED].base_edpi);


		RTE_LOG(INFO, POWER, "HIGH threshold is %"PRIu64"\n",
				poll_stats->thresh[HGH].base_edpi);

		RTE_LOG(INFO, POWER, "Training is Complete for %d\n",
				poll_stats->lcore_id);
	}

	return 0;
}

void
rte_empty_poll_detection(struct rte_timer *tim, void *arg)
{

	uint32_t i;

	struct priority_worker *poll_stats;

	RTE_SET_USED(tim);

	RTE_SET_USED(arg);

	for (i = 0; i < NUM_NODES; i++) {

		poll_stats = &(ep_params->wrk_data.wrk_stats[i]);

		if (rte_lcore_is_enabled(poll_stats->lcore_id) == 0)
			continue;

		switch (poll_stats->queue_state) {
		case(TRAINING):
			empty_poll_training(poll_stats,
					ep_params->max_train_iter);
			break;

		case(HGH_BUSY):
		case(MED_NORMAL):
			update_stats_normal(poll_stats);
			break;

		case(LOW_PURGE):
			break;
		default:
			break;

		}

	}

}

int
rte_power_empty_poll_stat_init(struct ep_params **eptr, uint8_t *freq_tlb,
		struct ep_policy *policy)
{
	uint32_t i;
	/* Allocate the ep_params structure */
	ep_params = rte_zmalloc_socket(NULL,
			sizeof(struct ep_params),
			0,
			rte_socket_id());

	if (!ep_params)
		return -1;

	if (freq_tlb == NULL) {
		freq_index[LOW] = 14;
		freq_index[MED] = 9;
		freq_index[HGH] = 1;
	} else {
		freq_index[LOW] = freq_tlb[LOW];
		freq_index[MED] = freq_tlb[MED];
		freq_index[HGH] = freq_tlb[HGH];
	}

	RTE_LOG(INFO, POWER, "Initialize the Empty Poll\n");

	/* Train for pre-defined period */
	ep_params->max_train_iter = INTERVALS_PER_SECOND * SECONDS_TO_TRAIN_FOR;

	struct stats_data *w = &ep_params->wrk_data;

	*eptr = ep_params;

	/* initialize all wrk_stats state */
	for (i = 0; i < NUM_NODES; i++) {

		if (rte_lcore_is_enabled(i) == 0)
			continue;
		/*init the freqs table */
		total_avail_freqs[i] = rte_power_freqs(i,
				avail_freqs[i],
				NUM_FREQS);

		RTE_LOG(INFO, POWER, "total avail freq is %d , lcoreid %d\n",
				total_avail_freqs[i],
				i);

		if (get_freq_index(LOW) > total_avail_freqs[i])
			return -1;

		if (rte_get_main_lcore() != i) {
			w->wrk_stats[i].lcore_id = i;
			set_policy(&w->wrk_stats[i], policy);
		}
	}

	return 0;
}

void
rte_power_empty_poll_stat_free(void)
{

	RTE_LOG(INFO, POWER, "Close the Empty Poll\n");

	if (ep_params != NULL)
		rte_free(ep_params);
}

int
rte_power_empty_poll_stat_update(unsigned int lcore_id)
{
	struct priority_worker *poll_stats;

	if (lcore_id >= NUM_NODES)
		return -1;

	poll_stats = &(ep_params->wrk_data.wrk_stats[lcore_id]);

	if (poll_stats->lcore_id == 0)
		poll_stats->lcore_id = lcore_id;

	poll_stats->empty_dequeues++;

	return 0;
}

int
rte_power_poll_stat_update(unsigned int lcore_id, uint8_t nb_pkt)
{

	struct priority_worker *poll_stats;

	if (lcore_id >= NUM_NODES)
		return -1;

	poll_stats = &(ep_params->wrk_data.wrk_stats[lcore_id]);

	if (poll_stats->lcore_id == 0)
		poll_stats->lcore_id = lcore_id;

	poll_stats->num_dequeue_pkts += nb_pkt;

	return 0;
}


uint64_t
rte_power_empty_poll_stat_fetch(unsigned int lcore_id)
{
	struct priority_worker *poll_stats;

	if (lcore_id >= NUM_NODES)
		return -1;

	poll_stats = &(ep_params->wrk_data.wrk_stats[lcore_id]);

	if (poll_stats->lcore_id == 0)
		poll_stats->lcore_id = lcore_id;

	return poll_stats->empty_dequeues;
}

uint64_t
rte_power_poll_stat_fetch(unsigned int lcore_id)
{
	struct priority_worker *poll_stats;

	if (lcore_id >= NUM_NODES)
		return -1;

	poll_stats = &(ep_params->wrk_data.wrk_stats[lcore_id]);

	if (poll_stats->lcore_id == 0)
		poll_stats->lcore_id = lcore_id;

	return poll_stats->num_dequeue_pkts;
}
