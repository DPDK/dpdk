/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 RehiveTech. All rights reserved.
 */

#ifndef _RTE_PREFETCH_ARM_H_
#define _RTE_PREFETCH_ARM_H_

#ifdef RTE_ARCH_64
#include <rte_prefetch_64.h>
#else
#include <rte_prefetch_32.h>
#endif

#endif /* _RTE_PREFETCH_ARM_H_ */
