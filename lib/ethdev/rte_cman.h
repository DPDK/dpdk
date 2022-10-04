/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell International Ltd.
 */

#ifndef RTE_CMAN_H
#define RTE_CMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_bitops.h>

/**
 * @file
 * Congestion management related parameters for DPDK.
 */

/** Congestion management modes */
enum rte_cman_mode {
	/**
	 * Congestion based on Random Early Detection.
	 *
	 * https://en.wikipedia.org/wiki/Random_early_detection
	 * http://www.aciri.org/floyd/papers/red/red.html
	 * @see struct rte_cman_red_params
	 */
	RTE_CMAN_RED = RTE_BIT32(0),
};

/**
 * RED based congestion management configuration parameters.
 */
struct rte_cman_red_params {
	/**
	 * Minimum threshold (min_th) value
	 *
	 * Value expressed as percentage. Value must be in 0 to 100(inclusive).
	 */
	uint8_t min_th;
	/**
	 * Maximum threshold (max_th) value
	 *
	 * Value expressed as percentage. Value must be in 0 to 100(inclusive).
	 */
	uint8_t max_th;
	/** Inverse of packet marking probability maximum value (maxp = 1 / maxp_inv) */
	uint16_t maxp_inv;
};

#ifdef __cplusplus
}
#endif

#endif /* RTE_CMAN_H */
