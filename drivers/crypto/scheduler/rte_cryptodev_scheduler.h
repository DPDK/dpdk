/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#ifndef _RTE_CRYPTO_SCHEDULER_H
#define _RTE_CRYPTO_SCHEDULER_H

#include <rte_cryptodev_scheduler_operations.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Crypto scheduler PMD operation modes
 */
enum rte_cryptodev_scheduler_mode {
	CDEV_SCHED_MODE_NOT_SET = 0,
	CDEV_SCHED_MODE_USERDEFINED,
	CDEV_SCHED_MODE_ROUNDROBIN,

	CDEV_SCHED_MODE_COUNT /* number of modes */
};

#define RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN	(64)
#define RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN	(256)

struct rte_cryptodev_scheduler;

/**
 * Load a user defined scheduler
 *
 * @param	scheduler_id	The target scheduler device ID
 *		scheduler	Pointer to the user defined scheduler
 *
 * @return
 *	0 if loading successful, negative integer if otherwise.
 */
int
rte_cryptodev_scheduler_load_user_scheduler(uint8_t scheduler_id,
		struct rte_cryptodev_scheduler *scheduler);

/**
 * Attach a pre-configured crypto device to the scheduler
 *
 * @param	scheduler_id	The target scheduler device ID
 *		slave_id	crypto device ID to be attached
 *
 * @return
 *	0 if attaching successful, negative int if otherwise.
 */
int
rte_cryptodev_scheduler_slave_attach(uint8_t scheduler_id, uint8_t slave_id);

/**
 * Detach a attached crypto device to the scheduler
 *
 * @param	scheduler_id	The target scheduler device ID
 *		slave_id	crypto device ID to be detached
 *
 * @return
 *	0 if detaching successful, negative int if otherwise.
 */
int
rte_cryptodev_scheduler_slave_detach(uint8_t scheduler_id, uint8_t slave_id);

/**
 * Set the scheduling mode
 *
 * @param	scheduler_id	The target scheduler device ID
 *		mode		The scheduling mode
 *
 * @return
 *	0 if attaching successful, negative integer if otherwise.
 */
int
rte_crpytodev_scheduler_mode_set(uint8_t scheduler_id,
		enum rte_cryptodev_scheduler_mode mode);

/**
 * Get the current scheduling mode
 *
 * @param	scheduler_id	The target scheduler device ID
 *		mode		Pointer to write the scheduling mode
 */
enum rte_cryptodev_scheduler_mode
rte_crpytodev_scheduler_mode_get(uint8_t scheduler_id);

/**
 * Set the crypto ops reordering feature on/off
 *
 * @param	dev_id		The target scheduler device ID
 *		enable_reorder	set the crypto op reordering feature
 *				0: disable reordering
 *				1: enable reordering
 *
 * @return
 *	0 if setting successful, negative integer if otherwise.
 */
int
rte_cryptodev_scheduler_ordering_set(uint8_t scheduler_id,
		uint32_t enable_reorder);

/**
 * Get the current crypto ops reordering feature
 *
 * @param	dev_id		The target scheduler device ID
 *
 * @return
 *	0 if reordering is disabled
 *	1 if reordering is enabled
 *	negative integer if otherwise.
 */
int
rte_cryptodev_scheduler_ordering_get(uint8_t scheduler_id);

typedef uint16_t (*rte_cryptodev_scheduler_burst_enqueue_t)(void *qp_ctx,
		struct rte_crypto_op **ops, uint16_t nb_ops);

typedef uint16_t (*rte_cryptodev_scheduler_burst_dequeue_t)(void *qp_ctx,
		struct rte_crypto_op **ops, uint16_t nb_ops);

struct rte_cryptodev_scheduler {
	const char *name;
	const char *description;
	enum rte_cryptodev_scheduler_mode mode;

	struct rte_cryptodev_scheduler_ops *ops;
};

extern struct rte_cryptodev_scheduler *roundrobin_scheduler;

#ifdef __cplusplus
}
#endif
#endif /* _RTE_CRYPTO_SCHEDULER_H */
