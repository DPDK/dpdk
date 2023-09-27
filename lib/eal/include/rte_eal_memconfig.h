/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_EAL_MEMCONFIG_H_
#define _RTE_EAL_MEMCONFIG_H_

#include <stdbool.h>

#include <rte_rwlock.h>
#include <rte_spinlock.h>

/**
 * @file
 *
 * This API allows access to EAL shared memory configuration through an API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Internal helpers used for lock annotations.
 */
__rte_internal
rte_rwlock_t *
rte_mcfg_mem_get_lock(void);

__rte_internal
rte_rwlock_t *
rte_mcfg_tailq_get_lock(void);

__rte_internal
rte_rwlock_t *
rte_mcfg_mempool_get_lock(void);

__rte_internal
rte_spinlock_t *
rte_mcfg_timer_get_lock(void);

__rte_internal
rte_spinlock_t *
rte_mcfg_ethdev_get_lock(void);

/**
 * Lock the internal EAL shared memory configuration for shared access.
 */
void
rte_mcfg_mem_read_lock(void)
	__rte_shared_lock_function(rte_mcfg_mem_get_lock());

/**
 * Unlock the internal EAL shared memory configuration for shared access.
 */
void
rte_mcfg_mem_read_unlock(void)
	__rte_unlock_function(rte_mcfg_mem_get_lock());

/**
 * Lock the internal EAL shared memory configuration for exclusive access.
 */
void
rte_mcfg_mem_write_lock(void)
	__rte_exclusive_lock_function(rte_mcfg_mem_get_lock());

/**
 * Unlock the internal EAL shared memory configuration for exclusive access.
 */
void
rte_mcfg_mem_write_unlock(void)
	__rte_unlock_function(rte_mcfg_mem_get_lock());

/**
 * Lock the internal EAL TAILQ list for shared access.
 */
void
rte_mcfg_tailq_read_lock(void)
	__rte_shared_lock_function(rte_mcfg_tailq_get_lock());

/**
 * Unlock the internal EAL TAILQ list for shared access.
 */
void
rte_mcfg_tailq_read_unlock(void)
	__rte_unlock_function(rte_mcfg_tailq_get_lock());

/**
 * Lock the internal EAL TAILQ list for exclusive access.
 */
void
rte_mcfg_tailq_write_lock(void)
	__rte_exclusive_lock_function(rte_mcfg_tailq_get_lock());

/**
 * Unlock the internal EAL TAILQ list for exclusive access.
 */
void
rte_mcfg_tailq_write_unlock(void)
	__rte_unlock_function(rte_mcfg_tailq_get_lock());

/**
 * Lock the internal EAL Mempool list for shared access.
 */
void
rte_mcfg_mempool_read_lock(void)
	__rte_shared_lock_function(rte_mcfg_mempool_get_lock());

/**
 * Unlock the internal EAL Mempool list for shared access.
 */
void
rte_mcfg_mempool_read_unlock(void)
	__rte_unlock_function(rte_mcfg_mempool_get_lock());

/**
 * Lock the internal EAL Mempool list for exclusive access.
 */
void
rte_mcfg_mempool_write_lock(void)
	__rte_exclusive_lock_function(rte_mcfg_mempool_get_lock());

/**
 * Unlock the internal EAL Mempool list for exclusive access.
 */
void
rte_mcfg_mempool_write_unlock(void)
	__rte_unlock_function(rte_mcfg_mempool_get_lock());

/**
 * Lock the internal EAL Timer Library lock for exclusive access.
 */
void
rte_mcfg_timer_lock(void)
	__rte_exclusive_lock_function(rte_mcfg_timer_get_lock());

/**
 * Unlock the internal EAL Timer Library lock for exclusive access.
 */
void
rte_mcfg_timer_unlock(void)
	__rte_unlock_function(rte_mcfg_timer_get_lock());

/**
 * If true, pages are put in single files (per memseg list),
 * as opposed to creating a file per page.
 */
bool
rte_mcfg_get_single_file_segments(void);

#ifdef __cplusplus
}
#endif

#endif /*__RTE_EAL_MEMCONFIG_H_*/
