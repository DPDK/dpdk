/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VMXNET3_OSDEP_H
#define _VMXNET3_OSDEP_H

#include <stdbool.h>

#include <rte_branch_prediction.h>

typedef uint64_t	uint64;
typedef uint32_t	uint32;
typedef uint16_t	uint16;
typedef uint8_t		uint8;

#ifndef UNLIKELY
#define UNLIKELY(x)  unlikely(x)
#endif /* unlikely */

#endif /* _VMXNET3_OSDEP_H */
