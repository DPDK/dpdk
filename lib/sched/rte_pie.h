/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef __RTE_PIE_H_INCLUDED__
#define __RTE_PIE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Proportional Integral controller Enhanced (PIE)
 **/

#include <stdint.h>

#include <rte_random.h>
#include <rte_debug.h>
#include <rte_cycles.h>

#define RTE_DQ_THRESHOLD   16384   /**< Queue length threshold (2^14)
				     * to start measurement cycle (bytes)
				     */
#define RTE_DQ_WEIGHT      0.25    /**< Weight (RTE_DQ_THRESHOLD/2^16) to compute dequeue rate */
#define RTE_ALPHA          0.125   /**< Weights in drop probability calculations */
#define RTE_BETA           1.25    /**< Weights in drop probability calculations */
#define RTE_RAND_MAX      ~0LLU    /**< Max value of the random number */


/**
 * PIE configuration parameters passed by user
 *
 */
struct rte_pie_params {
	uint16_t qdelay_ref;           /**< Latency Target (milliseconds) */
	uint16_t dp_update_interval;   /**< Update interval for drop probability (milliseconds) */
	uint16_t max_burst;            /**< Max Burst Allowance (milliseconds) */
	uint16_t tailq_th;             /**< Tailq drop threshold (packet counts) */
};

/**
 * PIE configuration parameters
 *
 */
struct rte_pie_config {
	uint64_t qdelay_ref;           /**< Latency Target (in CPU cycles.) */
	uint64_t dp_update_interval;   /**< Update interval for drop probability (in CPU cycles) */
	uint64_t max_burst;            /**< Max Burst Allowance (in CPU cycles.) */
	uint16_t tailq_th;             /**< Tailq drop threshold (packet counts) */
};

/**
 * PIE run-time data
 */
struct rte_pie {
	uint16_t active;               /**< Flag for activating/deactivating pie */
	uint16_t in_measurement;       /**< Flag for activation of measurement cycle */
	uint32_t departed_bytes_count; /**< Number of bytes departed in current measurement cycle */
	uint64_t start_measurement;    /**< Time to start to measurement cycle (in cpu cycles) */
	uint64_t last_measurement;     /**< Time of last measurement (in cpu cycles) */
	uint64_t qlen;                 /**< Queue length (packets count) */
	uint64_t qlen_bytes;           /**< Queue length (bytes count) */
	uint64_t avg_dq_time;          /**< Time averaged dequeue rate (in cpu cycles) */
	uint32_t burst_allowance;      /**< Current burst allowance (bytes) */
	uint64_t qdelay_old;           /**< Old queue delay (bytes) */
	double drop_prob;              /**< Current packet drop probability */
	double accu_prob;              /**< Accumulated packet drop probability */
};

/**
 * @brief Initialises run-time data
 *
 * @param pie [in,out] data pointer to PIE runtime data
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int
__rte_experimental
rte_pie_rt_data_init(struct rte_pie *pie);

/**
 * @brief Configures a single PIE configuration parameter structure.
 *
 * @param pie_cfg [in,out] config pointer to a PIE configuration parameter structure
 * @param qdelay_ref [in]  latency target(milliseconds)
 * @param dp_update_interval [in] update interval for drop probability (milliseconds)
 * @param max_burst [in] maximum burst allowance (milliseconds)
 * @param tailq_th [in] tail drop threshold for the queue (number of packets)
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int
__rte_experimental
rte_pie_config_init(struct rte_pie_config *pie_cfg,
	const uint16_t qdelay_ref,
	const uint16_t dp_update_interval,
	const uint16_t max_burst,
	const uint16_t tailq_th);

/**
 * @brief Decides packet enqueue when queue is empty
 *
 * Note: packet is never dropped in this particular case.
 *
 * @param pie_cfg [in] config pointer to a PIE configuration parameter structure
 * @param pie [in, out] data pointer to PIE runtime data
 * @param pkt_len [in] packet length in bytes
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval !0 drop the packet
 */
static int
__rte_experimental
rte_pie_enqueue_empty(const struct rte_pie_config *pie_cfg,
	struct rte_pie *pie,
	uint32_t pkt_len)
{
	RTE_ASSERT(pkt_len != 0);

	/* Update the PIE qlen parameter */
	pie->qlen++;
	pie->qlen_bytes += pkt_len;

	/**
	 * If the queue has been idle for a while, turn off PIE and Reset counters
	 */
	if ((pie->active == 1) &&
		(pie->qlen < (pie_cfg->tailq_th * 0.1))) {
		pie->active =  0;
		pie->in_measurement = 0;
	}

	return 0;
}

/**
 * @brief make a decision to drop or enqueue a packet based on probability
 *        criteria
 *
 * @param pie_cfg [in] config pointer to a PIE configuration parameter structure
 * @param pie [in, out] data pointer to PIE runtime data
 * @param time [in] current time (measured in cpu cycles)
 */
static void
__rte_experimental
_calc_drop_probability(const struct rte_pie_config *pie_cfg,
	struct rte_pie *pie, uint64_t time)
{
	uint64_t qdelay_ref = pie_cfg->qdelay_ref;

	/* Note: can be implemented using integer multiply.
	 * DQ_THRESHOLD is power of 2 value.
	 */
	uint64_t current_qdelay = pie->qlen * (pie->avg_dq_time >> 14);

	double p = RTE_ALPHA * (current_qdelay - qdelay_ref) +
		RTE_BETA * (current_qdelay - pie->qdelay_old);

	if (pie->drop_prob < 0.000001)
		p = p * 0.00048828125;              /* (1/2048) = 0.00048828125 */
	else if (pie->drop_prob < 0.00001)
		p = p * 0.001953125;                /* (1/512) = 0.001953125  */
	else if (pie->drop_prob < 0.0001)
		p = p * 0.0078125;                  /* (1/128) = 0.0078125  */
	else if (pie->drop_prob < 0.001)
		p = p * 0.03125;                    /* (1/32) = 0.03125   */
	else if (pie->drop_prob < 0.01)
		p = p * 0.125;                      /* (1/8) = 0.125    */
	else if (pie->drop_prob < 0.1)
		p = p * 0.5;                        /* (1/2) = 0.5    */

	if (pie->drop_prob >= 0.1 && p > 0.02)
		p = 0.02;

	pie->drop_prob += p;

	double qdelay = qdelay_ref * 0.5;

	/*  Exponentially decay drop prob when congestion goes away  */
	if ((double)current_qdelay < qdelay && pie->qdelay_old < qdelay)
		pie->drop_prob *= 0.98;     /* 1 - 1/64 is sufficient */

	/* Bound drop probability */
	if (pie->drop_prob < 0)
		pie->drop_prob = 0;
	if (pie->drop_prob > 1)
		pie->drop_prob = 1;

	pie->qdelay_old = current_qdelay;
	pie->last_measurement = time;

	uint64_t burst_allowance = pie->burst_allowance - pie_cfg->dp_update_interval;

	pie->burst_allowance = (burst_allowance > 0) ? burst_allowance : 0;
}

/**
 * @brief make a decision to drop or enqueue a packet based on probability
 *        criteria
 *
 * @param pie_cfg [in] config pointer to a PIE configuration parameter structure
 * @param pie [in, out] data pointer to PIE runtime data
 *
 * @return operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet
 */
static inline int
__rte_experimental
_rte_pie_drop(const struct rte_pie_config *pie_cfg,
	struct rte_pie *pie)
{
	uint64_t rand_value;
	uint64_t qdelay = pie_cfg->qdelay_ref / 2;

	/* PIE is active but the queue is not congested: return 0 */
	if (((pie->qdelay_old < qdelay) && (pie->drop_prob < 0.2)) ||
		(pie->qlen <= (pie_cfg->tailq_th * 0.1)))
		return 0;

	if (pie->drop_prob == 0)
		pie->accu_prob = 0;

	/* For practical reasons, drop probability can be further scaled according
	 * to packet size, but one needs to set a bound to avoid unnecessary bias
	 * Random drop
	 */
	pie->accu_prob += pie->drop_prob;

	if (pie->accu_prob < 0.85)
		return 0;

	if (pie->accu_prob >= 8.5)
		return 1;

	rand_value = rte_rand()/RTE_RAND_MAX;

	if ((double)rand_value < pie->drop_prob) {
		pie->accu_prob = 0;
		return 1;
	}

	/* No drop */
	return 0;
}

/**
 * @brief Decides if new packet should be enqueued or dropped for non-empty queue
 *
 * @param pie_cfg [in] config pointer to a PIE configuration parameter structure
 * @param pie [in,out] data pointer to PIE runtime data
 * @param pkt_len [in] packet length in bytes
 * @param time [in] current time (measured in cpu cycles)
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criterion
 * @retval 2 drop the packet based on mark probability criterion
 */
static inline int
__rte_experimental
rte_pie_enqueue_nonempty(const struct rte_pie_config *pie_cfg,
	struct rte_pie *pie,
	uint32_t pkt_len,
	const uint64_t time)
{
	/* Check queue space against the tail drop threshold */
	if (pie->qlen >= pie_cfg->tailq_th) {

		pie->accu_prob = 0;
		return 1;
	}

	if (pie->active) {
		/* Update drop probability after certain interval */
		if ((time - pie->last_measurement) >= pie_cfg->dp_update_interval)
			_calc_drop_probability(pie_cfg, pie, time);

		/* Decide whether packet to be dropped or enqueued */
		if (_rte_pie_drop(pie_cfg, pie) && pie->burst_allowance == 0)
			return 2;
	}

	/* When queue occupancy is over a certain threshold, turn on PIE */
	if ((pie->active == 0) &&
		(pie->qlen >= (pie_cfg->tailq_th * 0.1))) {
		pie->active = 1;
		pie->qdelay_old = 0;
		pie->drop_prob = 0;
		pie->in_measurement = 1;
		pie->departed_bytes_count = 0;
		pie->avg_dq_time = 0;
		pie->last_measurement = time;
		pie->burst_allowance = pie_cfg->max_burst;
		pie->accu_prob = 0;
		pie->start_measurement = time;
	}

	/* when queue has been idle for a while, turn off PIE and Reset counters */
	if (pie->active == 1 &&
		pie->qlen < (pie_cfg->tailq_th * 0.1)) {
		pie->active =  0;
		pie->in_measurement = 0;
	}

	/* Update PIE qlen parameter */
	pie->qlen++;
	pie->qlen_bytes += pkt_len;

	/* No drop */
	return 0;
}

/**
 * @brief Decides if new packet should be enqueued or dropped
 * Updates run time data and gives verdict whether to enqueue or drop the packet.
 *
 * @param pie_cfg [in] config pointer to a PIE configuration parameter structure
 * @param pie [in,out] data pointer to PIE runtime data
 * @param qlen [in] queue length
 * @param pkt_len [in] packet length in bytes
 * @param time [in] current time stamp (measured in cpu cycles)
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on drop probability criteria
 */
static inline int
__rte_experimental
rte_pie_enqueue(const struct rte_pie_config *pie_cfg,
	struct rte_pie *pie,
	const unsigned int qlen,
	uint32_t pkt_len,
	const uint64_t time)
{
	RTE_ASSERT(pie_cfg != NULL);
	RTE_ASSERT(pie != NULL);

	if (qlen != 0)
		return rte_pie_enqueue_nonempty(pie_cfg, pie, pkt_len, time);
	else
		return rte_pie_enqueue_empty(pie_cfg, pie, pkt_len);
}

/**
 * @brief PIE rate estimation method
 * Called on each packet departure.
 *
 * @param pie [in] data pointer to PIE runtime data
 * @param pkt_len [in] packet length in bytes
 * @param time [in] current time stamp in cpu cycles
 */
static inline void
__rte_experimental
rte_pie_dequeue(struct rte_pie *pie,
	uint32_t pkt_len,
	uint64_t time)
{
	/* Dequeue rate estimation */
	if (pie->in_measurement) {
		pie->departed_bytes_count += pkt_len;

		/* Start a new measurement cycle when enough packets */
		if (pie->departed_bytes_count >= RTE_DQ_THRESHOLD) {
			uint64_t dq_time = time - pie->start_measurement;

			if (pie->avg_dq_time == 0)
				pie->avg_dq_time = dq_time;
			else
				pie->avg_dq_time = dq_time * RTE_DQ_WEIGHT + pie->avg_dq_time
					* (1 - RTE_DQ_WEIGHT);

			pie->in_measurement = 0;
		}
	}

	/* Start measurement cycle when enough data in the queue */
	if ((pie->qlen_bytes >= RTE_DQ_THRESHOLD) && (pie->in_measurement == 0)) {
		pie->in_measurement = 1;
		pie->start_measurement = time;
		pie->departed_bytes_count = 0;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* __RTE_PIE_H_INCLUDED__ */
