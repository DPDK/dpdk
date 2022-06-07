/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 */

#ifndef RTE_RWLOCK_RISCV_H
#define RTE_RWLOCK_RISCV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_rwlock.h"

static inline void
rte_rwlock_read_lock_tm(rte_rwlock_t *rwl)
{
	rte_rwlock_read_lock(rwl);
}

static inline void
rte_rwlock_read_unlock_tm(rte_rwlock_t *rwl)
{
	rte_rwlock_read_unlock(rwl);
}

static inline void
rte_rwlock_write_lock_tm(rte_rwlock_t *rwl)
{
	rte_rwlock_write_lock(rwl);
}

static inline void
rte_rwlock_write_unlock_tm(rte_rwlock_t *rwl)
{
	rte_rwlock_write_unlock(rwl);
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_RWLOCK_RISCV_H */
