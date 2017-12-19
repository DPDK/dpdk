/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_LATENCYSTATS_H_
#define _RTE_LATENCYSTATS_H_

/**
 * @file
 * RTE latency stats
 *
 * library to provide application and flow based latency stats.
 */

#include <stdint.h>
#include <rte_metrics.h>
#include <rte_mbuf.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Note: This function pointer is for future flow based latency stats
 *  implementation.
 *
 * Function type used for identifting flow types of a Rx packet.
 *
 * The callback function is called on Rx for each packet.
 * This function is used for flow based latency calculations.
 *
 * @param pkt
 *   Packet that has to be identified with its flow types.
 * @param user_param
 *   The arbitrary user parameter passed in by the application when
 *   the callback was originally configured.
 * @return
 *   The flow_mask, representing the multiple flow types of a packet.
 */
typedef uint16_t (*rte_latency_stats_flow_type_fn)(struct rte_mbuf *pkt,
							void *user_param);

/**
 *  Registers Rx/Tx callbacks for each active port, queue.
 *
 * @param samp_intvl
 *  Sampling time period in nano seconds, at which packet
 *  should be marked with time stamp.
 * @param user_cb
 *  Note: This param is for future flow based latency stats
 *  implementation.
 *  User callback to be called to get flow types of a packet.
 *  Used for flow based latency calculation.
 *  If the value is NULL, global stats will be calculated,
 *  else flow based latency stats will be calculated.
 *  For now just pass on the NULL value to this param.
 *  @return
 *   -1     : On error
 *   -ENOMEM: On error
 *    0     : On success
 */
int rte_latencystats_init(uint64_t samp_intvl,
			rte_latency_stats_flow_type_fn user_cb);

/**
 * Calculates the latency and jitter values internally, exposing the updated
 * values via *rte_latencystats_get* or the rte_metrics API.
 * @return:
 *  0      : on Success
 *  < 0    : Error in updating values.
 */
int32_t rte_latencystats_update(void);

/**
 *  Removes registered Rx/Tx callbacks for each active port, queue.
 *
 *  @return
 *   -1: On error
 *    0: On success
 */
int rte_latencystats_uninit(void);

/**
 * Retrieve names of latency statistics
 *
 * @param names
 *  Block of memory to insert names into. Must be at least size in capacity.
 *  If set to NULL, function returns required capacity.
 * @param size
 *  Capacity of latency stats names (number of names).
 * @return
 *   - positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 */
int rte_latencystats_get_names(struct rte_metric_name *names,
				uint16_t size);

/**
 * Retrieve latency statistics.
 *
 * @param values
 *   A pointer to a table of structure of type *rte_metric_value*
 *   to be filled with latency statistics ids and values.
 *   This parameter can be set to NULL if size is 0.
 * @param size
 *   The size of the stats table, which should be large enough to store
 *   all the latency stats.
 * @return
 *   - positive value lower or equal to size: success. The return value
 *     is the number of entries filled in the stats table.
 *   - positive value higher than size: error, the given statistics table
 *     is too small. The return value corresponds to the size that should
 *     be given to succeed. The entries in the table are not valid and
 *     shall not be used by the caller.
 *   -ENOMEM: On failure.
 */
int rte_latencystats_get(struct rte_metric_value *values,
			uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LATENCYSTATS_H_ */
