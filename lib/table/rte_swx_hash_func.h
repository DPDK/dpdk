/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_HASH_FUNC_H__
#define __INCLUDE_RTE_SWX_HASH_FUNC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Hash Function
 */

#include <stdint.h>

/**
 * Hash function prototype
 *
 * @param[in] key
 *   Key to hash. Must be non-NULL.
 * @param[in] length
 *   Key length in bytes.
 * @param[in] seed
 *   Hash seed.
 * @return
 *   Hash value.
 */
typedef uint32_t
(*rte_swx_hash_func_t)(const void *key,
		       uint32_t length,
		       uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif
