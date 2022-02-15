/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __RTE_RED_H_INCLUDED__
#define __RTE_RED_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Random Early Detection (RED)
 *
 *
 ***/

#include <stdint.h>
#include <limits.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_branch_prediction.h>

#define RTE_RED_SCALING                     10         /**< Fraction size for fixed-point */
#define RTE_RED_S                           (1 << 22)  /**< Packet size multiplied by number of leaf queues */
#define RTE_RED_MAX_TH_MAX                  1023       /**< Max threshold limit in fixed point format */
#define RTE_RED_WQ_LOG2_MIN                 1          /**< Min inverse filter weight value */
#define RTE_RED_WQ_LOG2_MAX                 12         /**< Max inverse filter weight value */
#define RTE_RED_MAXP_INV_MIN                1          /**< Min inverse mark probability value */
#define RTE_RED_MAXP_INV_MAX                255        /**< Max inverse mark probability value */
#define RTE_RED_2POW16                      (1<<16)    /**< 2 power 16 */
#define RTE_RED_INT16_NBITS                 (sizeof(uint16_t) * CHAR_BIT)
#define RTE_RED_WQ_LOG2_NUM                 (RTE_RED_WQ_LOG2_MAX - RTE_RED_WQ_LOG2_MIN + 1)

/**
 * Externs
 *
 */
extern uint32_t rte_red_rand_val;
extern uint32_t rte_red_rand_seed;
extern uint16_t rte_red_log2_1_minus_Wq[RTE_RED_WQ_LOG2_NUM];
extern uint16_t rte_red_pow2_frac_inv[16];

/**
 * RED configuration parameters passed by user
 *
 */
struct rte_red_params {
	uint16_t min_th;   /**< Minimum threshold for queue (max_th) */
	uint16_t max_th;   /**< Maximum threshold for queue (max_th) */
	uint16_t maxp_inv; /**< Inverse of packet marking probability maximum value (maxp = 1 / maxp_inv) */
	uint16_t wq_log2;  /**< Negated log2 of queue weight (wq = 1 / (2 ^ wq_log2)) */
};

/**
 * RED configuration parameters
 */
struct rte_red_config {
	uint32_t min_th;   /**< min_th scaled in fixed-point format */
	uint32_t max_th;   /**< max_th scaled in fixed-point format */
	uint32_t pa_const; /**< Precomputed constant value used for pa calculation (scaled in fixed-point format) */
	uint8_t maxp_inv;  /**< maxp_inv */
	uint8_t wq_log2;   /**< wq_log2 */
};

/**
 * RED run-time data
 */
struct rte_red {
	uint32_t avg;      /**< Average queue size (avg), scaled in fixed-point format */
	uint32_t count;    /**< Number of packets since last marked packet (count) */
	uint64_t q_time;   /**< Start of the queue idle time (q_time) */
};

/**
 * @brief Initialises run-time data
 *
 * @param red [in,out] data pointer to RED runtime data
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int
rte_red_rt_data_init(struct rte_red *red);

/**
 * @brief Configures a single RED configuration parameter structure.
 *
 * @param red_cfg [in,out] config pointer to a RED configuration parameter structure
 * @param wq_log2 [in]  log2 of the filter weight, valid range is:
 *             RTE_RED_WQ_LOG2_MIN <= wq_log2 <= RTE_RED_WQ_LOG2_MAX
 * @param min_th [in] queue minimum threshold in number of packets
 * @param max_th [in] queue maximum threshold in number of packets
 * @param maxp_inv [in] inverse maximum mark probability
 *
 * @return Operation status
 * @retval 0 success
 * @retval !0 error
 */
int
rte_red_config_init(struct rte_red_config *red_cfg,
	const uint16_t wq_log2,
	const uint16_t min_th,
	const uint16_t max_th,
	const uint16_t maxp_inv);

/**
 * @brief Generate random number for RED
 *
 * Implementation based on:
 * http://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/
 *
 * 10 bit shift has been found through empirical tests (was 16).
 *
 * @return Random number between 0 and (2^22 - 1)
 */
static inline uint32_t
rte_fast_rand(void)
{
	rte_red_rand_seed = (214013 * rte_red_rand_seed) + 2531011;
	return rte_red_rand_seed >> 10;
}

/**
 * @brief calculate factor to scale average queue size when queue
 *        becomes empty
 *
 * @param wq_log2 [in] where EWMA filter weight wq = 1/(2 ^ wq_log2)
 * @param m [in] exponent in the computed value (1 - wq) ^ m
 *
 * @return computed value
 * @retval ((1 - wq) ^ m) scaled in fixed-point format
 */
static inline uint16_t
__rte_red_calc_qempty_factor(uint8_t wq_log2, uint16_t m)
{
	uint32_t n = 0;
	uint32_t f = 0;

	/**
	 * Basic math tells us that:
	 *   a^b = 2^(b * log2(a) )
	 *
	 * in our case:
	 *   a = (1-Wq)
	 *   b = m
	 *  Wq = 1/ (2^log2n)
	 *
	 * So we are computing this equation:
	 *   factor = 2 ^ ( m * log2(1-Wq))
	 *
	 * First we are computing:
	 *    n = m * log2(1-Wq)
	 *
	 * To avoid dealing with signed numbers log2 values are positive
	 * but they should be negative because (1-Wq) is always < 1.
	 * Contents of log2 table values are also scaled for precision.
	 */

	n = m * rte_red_log2_1_minus_Wq[wq_log2 - RTE_RED_WQ_LOG2_MIN];

	/**
	 * The tricky part is computing 2^n, for this I split n into
	 * integer part and fraction part.
	 *   f - is fraction part of n
	 *   n - is integer part of original n
	 *
	 * Now using basic math we compute 2^n:
	 *   2^(f+n) = 2^f * 2^n
	 *   2^f - we use lookup table
	 *   2^n - can be replaced with bit shift right operations
	 */

	f = (n >> 6) & 0xf;
	n >>= 10;

	if (n < RTE_RED_SCALING)
		return (uint16_t) ((rte_red_pow2_frac_inv[f] + (1 << (n - 1))) >> n);

	return 0;
}

/**
 * @brief Updates queue average in condition when queue is empty
 *
 * Note: packet is never dropped in this particular case.
 *
 * @param red_cfg [in] config pointer to a RED configuration parameter structure
 * @param red [in,out] data pointer to RED runtime data
 * @param time [in] current time stamp
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criterion
 * @retval 2 drop the packet based on mark probability criterion
 */
static inline int
rte_red_enqueue_empty(const struct rte_red_config *red_cfg,
	struct rte_red *red,
	const uint64_t time)
{
	uint64_t time_diff = 0, m = 0;

	RTE_ASSERT(red_cfg != NULL);
	RTE_ASSERT(red != NULL);

	red->count ++;

	/**
	 * We compute avg but we don't compare avg against
	 *  min_th or max_th, nor calculate drop probability
	 */
	time_diff = time - red->q_time;

	/**
	 * m is the number of packets that might have arrived while the queue was empty.
	 * In this case we have time stamps provided by scheduler in byte units (bytes
	 * transmitted on network port). Such time stamp translates into time units as
	 * port speed is fixed but such approach simplifies the code.
	 */
	m = time_diff / RTE_RED_S;

	/**
	 * Check that m will fit into 16-bit unsigned integer
	 */
	if (m >= RTE_RED_2POW16) {
		red->avg = 0;
	} else {
		red->avg = (red->avg >> RTE_RED_SCALING) * __rte_red_calc_qempty_factor(red_cfg->wq_log2, (uint16_t) m);
	}

	return 0;
}

/**
 *  Drop probability (Sally Floyd and Van Jacobson):
 *
 *     pb = (1 / maxp_inv) * (avg - min_th) / (max_th - min_th)
 *     pa = pb / (2 - count * pb)
 *
 *
 *                 (1 / maxp_inv) * (avg - min_th)
 *                ---------------------------------
 *                         max_th - min_th
 *     pa = -----------------------------------------------
 *                count * (1 / maxp_inv) * (avg - min_th)
 *           2 - -----------------------------------------
 *                          max_th - min_th
 *
 *
 *                                  avg - min_th
 *     pa = -----------------------------------------------------------
 *           2 * (max_th - min_th) * maxp_inv - count * (avg - min_th)
 *
 *
 *  We define pa_const as: pa_const =  2 * (max_th - min_th) * maxp_inv. Then:
 *
 *
 *                     avg - min_th
 *     pa = -----------------------------------
 *           pa_const - count * (avg - min_th)
 */

/**
 * @brief make a decision to drop or enqueue a packet based on mark probability
 *        criteria
 *
 * @param red_cfg [in] config pointer to structure defining RED parameters
 * @param red [in,out] data pointer to RED runtime data
 *
 * @return operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet
 */
static inline int
__rte_red_drop(const struct rte_red_config *red_cfg, struct rte_red *red)
{
	uint32_t pa_num = 0;    /* numerator of drop-probability */
	uint32_t pa_den = 0;    /* denominator of drop-probability */
	uint32_t pa_num_count = 0;

	pa_num = (red->avg - red_cfg->min_th) >> (red_cfg->wq_log2);

	pa_num_count = red->count * pa_num;

	if (red_cfg->pa_const <= pa_num_count)
		return 1;

	pa_den = red_cfg->pa_const - pa_num_count;

	/* If drop, generate and save random number to be used next time */
	if (unlikely((rte_red_rand_val % pa_den) < pa_num)) {
		rte_red_rand_val = rte_fast_rand();

		return 1;
	}

	/* No drop */
	return 0;
}

/**
 * @brief Decides if new packet should be enqueued or dropped in queue non-empty case
 *
 * @param red_cfg [in] config pointer to a RED configuration parameter structure
 * @param red [in,out] data pointer to RED runtime data
 * @param q [in] current queue size (measured in packets)
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criterion
 * @retval 2 drop the packet based on mark probability criterion
 */
static inline int
rte_red_enqueue_nonempty(const struct rte_red_config *red_cfg,
	struct rte_red *red,
	const unsigned q)
{
	RTE_ASSERT(red_cfg != NULL);
	RTE_ASSERT(red != NULL);

	/**
	* EWMA filter (Sally Floyd and Van Jacobson):
	*    avg = (1 - wq) * avg + wq * q
	*    avg = avg + q * wq - avg * wq
	*
	* We select: wq = 2^(-n). Let scaled version of avg be: avg_s = avg * 2^(N+n). We get:
	*    avg_s = avg_s + q * 2^N - avg_s * 2^(-n)
	*
	* By using shift left/right operations, we get:
	*    avg_s = avg_s + (q << N) - (avg_s >> n)
	*    avg_s += (q << N) - (avg_s >> n)
	*/

	/* avg update */
	red->avg += (q << RTE_RED_SCALING) - (red->avg >> red_cfg->wq_log2);

	/* avg < min_th: do not mark the packet  */
	if (red->avg < red_cfg->min_th) {
		red->count ++;
		return 0;
	}

	/* min_th <= avg < max_th: mark the packet with pa probability */
	if (red->avg < red_cfg->max_th) {
		if (!__rte_red_drop(red_cfg, red)) {
			red->count ++;
			return 0;
		}

		red->count = 0;
		return 2;
	}

	/* max_th <= avg: always mark the packet */
	red->count = 0;
	return 1;
}

/**
 * @brief Decides if new packet should be enqueued or dropped
 * Updates run time data based on new queue size value.
 * Based on new queue average and RED configuration parameters
 * gives verdict whether to enqueue or drop the packet.
 *
 * @param red_cfg [in] config pointer to a RED configuration parameter structure
 * @param red [in,out] data pointer to RED runtime data
 * @param q [in] updated queue size in packets
 * @param time [in] current time stamp
 *
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criteria
 * @retval 2 drop the packet based on mark probability criteria
 */
static inline int
rte_red_enqueue(const struct rte_red_config *red_cfg,
	struct rte_red *red,
	const unsigned q,
	const uint64_t time)
{
	RTE_ASSERT(red_cfg != NULL);
	RTE_ASSERT(red != NULL);

	if (q != 0) {
		return rte_red_enqueue_nonempty(red_cfg, red, q);
	} else {
		return rte_red_enqueue_empty(red_cfg, red, time);
	}
}

/**
 * @brief Callback to records time that queue became empty
 *
 * @param red [in,out] data pointer to RED runtime data
 * @param time [in] current time stamp
 */
static inline void
rte_red_mark_queue_empty(struct rte_red *red, const uint64_t time)
{
	red->q_time = time;
}

#ifdef __cplusplus
}
#endif

#endif /* __RTE_RED_H_INCLUDED__ */
