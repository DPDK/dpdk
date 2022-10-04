/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Loongson Technology Corporation Limited
 */

#ifndef RTE_PAUSE_LOONGARCH_H
#define RTE_PAUSE_LOONGARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_atomic.h"

#include "generic/rte_pause.h"

static inline void rte_pause(void)
{
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PAUSE_LOONGARCH_H */
