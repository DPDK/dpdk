/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_EAL_MEMCONFIG_H_
#define _RTE_EAL_MEMCONFIG_H_

/**
 * @file
 *
 * This API allows access to EAL shared memory configuration through an API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lock the internal EAL shared memory configuration for shared access.
 */
void
rte_mcfg_mem_read_lock(void);

/**
 * Unlock the internal EAL shared memory configuration for shared access.
 */
void
rte_mcfg_mem_read_unlock(void);

/**
 * Lock the internal EAL shared memory configuration for exclusive access.
 */
void
rte_mcfg_mem_write_lock(void);

/**
 * Unlock the internal EAL shared memory configuration for exclusive access.
 */
void
rte_mcfg_mem_write_unlock(void);

/**
 * Lock the internal EAL TAILQ list for shared access.
 */
void
rte_mcfg_tailq_read_lock(void);

/**
 * Unlock the internal EAL TAILQ list for shared access.
 */
void
rte_mcfg_tailq_read_unlock(void);

/**
 * Lock the internal EAL TAILQ list for exclusive access.
 */
void
rte_mcfg_tailq_write_lock(void);

/**
 * Unlock the internal EAL TAILQ list for exclusive access.
 */
void
rte_mcfg_tailq_write_unlock(void);

/**
 * Lock the internal EAL Mempool list for shared access.
 */
void
rte_mcfg_mempool_read_lock(void);

/**
 * Unlock the internal EAL Mempool list for shared access.
 */
void
rte_mcfg_mempool_read_unlock(void);

/**
 * Lock the internal EAL Mempool list for exclusive access.
 */
void
rte_mcfg_mempool_write_lock(void);

/**
 * Unlock the internal EAL Mempool list for exclusive access.
 */
void
rte_mcfg_mempool_write_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /*__RTE_EAL_MEMCONFIG_H_*/
