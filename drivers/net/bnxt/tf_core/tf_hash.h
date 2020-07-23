/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Broadcom
 * All rights reserved.
 */

#ifndef _TF_HASH_H_
#define _TF_HASH_H_

#include "tf_core.h"

/**
 * Calculate a crc32 on the buffer with an initial value and len
 *
 * Returns the crc32
 */
uint32_t
tf_hash_calc_crc32i(uint32_t init, uint8_t *buf, uint32_t len);

/**
 * Calculate a crc32 on the buffer with a default initial value
 *
 * Returns the crc32
 */
uint32_t
tf_hash_calc_crc32(uint8_t *buf, uint32_t len);

#endif
