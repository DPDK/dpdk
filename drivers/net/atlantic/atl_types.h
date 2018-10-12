/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */
#ifndef ATL_TYPES_H
#define ATL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>


typedef uint8_t		u8;
typedef int8_t		s8;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint64_t	u64;

#define min(a, b)	RTE_MIN(a, b)
#define max(a, b)	RTE_MAX(a, b)

#endif
