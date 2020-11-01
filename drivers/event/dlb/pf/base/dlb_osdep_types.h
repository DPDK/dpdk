/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef __DLB_OSDEP_TYPES_H
#define __DLB_OSDEP_TYPES_H

#include <linux/types.h>

#include <inttypes.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Types for user mode PF PMD */
typedef uint8_t         u8;
typedef int8_t          s8;
typedef uint16_t        u16;
typedef int16_t         s16;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint64_t        u64;

#define __iomem

/* END types for user mode PF PMD */

#endif /* __DLB_OSDEP_TYPES_H */
