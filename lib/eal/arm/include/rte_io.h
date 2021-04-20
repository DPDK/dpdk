/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _RTE_IO_ARM_H_
#define _RTE_IO_ARM_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTE_ARCH_64
#include "rte_io_64.h"
#else
#include "generic/rte_io.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IO_ARM_H_ */
