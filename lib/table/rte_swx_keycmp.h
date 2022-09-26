/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_KEYCMP_H__
#define __INCLUDE_RTE_SWX_KEYCMP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Key Comparison Functions
 */

#include <stdint.h>
#include <string.h>

/**
 * Key comparison function prototype
 *
 * @param[in] key1
 *   First key to compare. Must be non-NULL.
 * @param[in] key2
 *   Second key to compare. Must be non-NULL.
 * @param[in] key_size
 *   Key size in bytes.
 * @return
 *   0 when keys are different, 1 when keys are equal.
 */
typedef uint32_t
(*rte_swx_keycmp_func_t)(void *key1, void *key2, uint32_t key_size);

/**
 * Key comparison function get
 *
 * @param[in] key_size
 *   Key size in bytes.
 * @return
 *   Key comparison function for the given key size
 */
rte_swx_keycmp_func_t
rte_swx_keycmp_func_get(uint32_t key_size);

#ifdef __cplusplus
}
#endif

#endif
