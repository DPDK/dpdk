/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _RTE_THASH_GFNI_H_
#define _RTE_THASH_GFNI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_log.h>

#ifdef RTE_ARCH_X86

#include <rte_thash_x86_gfni.h>

#endif

#ifndef RTE_THASH_GFNI_DEFINED

/**
 * Calculate Toeplitz hash.
 * Dummy implementation.
 *
 * @param m
 *  Pointer to the matrices generated from the corresponding
 *  RSS hash key using rte_thash_complete_matrix().
 * @param tuple
 *  Pointer to the data to be hashed. Data must be in network byte order.
 * @param len
 *  Length of the data to be hashed.
 * @return
 *  Calculated Toeplitz hash value.
 */
uint32_t
rte_thash_gfni(const uint64_t *mtrx, const uint8_t *key, int len);

/**
 * Bulk implementation for Toeplitz hash.
 * Dummy implementation.
 *
 * @param m
 *  Pointer to the matrices generated from the corresponding
 *  RSS hash key using rte_thash_complete_matrix().
 * @param len
 *  Length of the largest data buffer to be hashed.
 * @param tuple
 *  Array of the pointers on data to be hashed.
 *  Data must be in network byte order.
 * @param val
 *  Array of uint32_t where to put calculated Toeplitz hash values
 * @param num
 *  Number of tuples to hash.
 */
void
rte_thash_gfni_bulk(const uint64_t *mtrx, int len, uint8_t *tuple[],
	uint32_t val[], uint32_t num);

#endif /* RTE_THASH_GFNI_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THASH_GFNI_H_ */
