/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_BITRATE_H_
#define _RTE_BITRATE_H_

#include <stdint.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Bitrate statistics data structure.
 *  This data structure is intentionally opaque.
 */
struct rte_stats_bitrates;


/**
 * Allocate a bitrate statistics structure
 *
 * @return
 *   - Pointer to structure on success
 *   - NULL on error (zmalloc failure)
 */
struct rte_stats_bitrates *rte_stats_bitrate_create(void);

/**
 * Free bitrate statistics structure
 *
 * @param bitrate_data
 *   Pointer allocated by rte_stats_bitrate_create()
 */
__rte_experimental
void rte_stats_bitrate_free(struct rte_stats_bitrates *bitrate_data);

/**
 * Register bitrate statistics with the metric library.
 *
 * @param bitrate_data
 *   Pointer allocated by rte_stats_bitrate_create()
 *
 * @return
 *   Zero on success
 *   Negative on error
 */
int rte_stats_bitrate_reg(struct rte_stats_bitrates *bitrate_data);


/**
 * Calculate statistics for current time window. The period with which
 * this function is called should be the intended sampling window width.
 *
 * @param bitrate_data
 *   Bitrate statistics data pointer
 *
 * @param port_id
 *   Port id to calculate statistics for
 *
 * @return
 *  - Zero on success
 *  - Negative value on error
 */
int rte_stats_bitrate_calc(struct rte_stats_bitrates *bitrate_data,
			   uint16_t port_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BITRATE_H_ */
