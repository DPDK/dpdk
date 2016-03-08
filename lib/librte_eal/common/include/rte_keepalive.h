/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 Intel Shannon Ltd. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file rte_keepalive.h
 * DPDK RTE LCore Keepalive Monitor.
 *
 **/

#ifndef _KEEPALIVE_H_
#define _KEEPALIVE_H_

#include <rte_memory.h>

#ifndef RTE_KEEPALIVE_MAXCORES
/**
 * Number of cores to track.
 * @note Must be larger than the highest core id. */
#define RTE_KEEPALIVE_MAXCORES RTE_MAX_LCORE
#endif

/**
 * Keepalive failure callback.
 *
 *  Receives a data pointer passed to rte_keepalive_create() and the id of the
 *  failed core.
 */
typedef void (*rte_keepalive_failure_callback_t)(
	void *data,
	const int id_core);

/**
 * Keepalive state structure.
 * @internal
 */
struct rte_keepalive;

/**
 * Initialise keepalive sub-system.
 * @param callback
 *   Function called upon detection of a dead core.
 * @param data
 *   Data pointer to be passed to function callback.
 * @return
 *   Keepalive structure success, NULL on failure.
 */
struct rte_keepalive *rte_keepalive_create(
	rte_keepalive_failure_callback_t callback,
	void *data);

/**
 * Checks & handles keepalive state of monitored cores.
 * @param *ptr_timer Triggering timer (unused)
 * @param *ptr_data  Data pointer (keepalive structure)
 */
void rte_keepalive_dispatch_pings(void *ptr_timer, void *ptr_data);

/**
 * Registers a core for keepalive checks.
 * @param *keepcfg
 *   Keepalive structure pointer
 * @param id_core
 *   ID number of core to register.
 */
void rte_keepalive_register_core(struct rte_keepalive *keepcfg,
	const int id_core);

/**
 * Per-core keepalive check.
 * @param *keepcfg
 *   Keepalive structure pointer
 *
 * This function needs to be called from within the main process loop of
 * the LCore to be checked.
 */
void
rte_keepalive_mark_alive(struct rte_keepalive *keepcfg);

#endif /* _KEEPALIVE_H_ */
