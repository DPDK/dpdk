/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_CRYPTO_SCHEDULER_H
#define _RTE_CRYPTO_SCHEDULER_H

/**
 * @file rte_cryptodev_scheduler.h
 *
 * RTE Cryptodev Scheduler Device
 *
 * The RTE Cryptodev Scheduler Device allows the aggregation of multiple (slave)
 * Cryptodevs into a single logical crypto device, and the scheduling the
 * crypto operations to the slaves based on the mode of the specified mode of
 * operation specified and supported. This implementation supports 3 modes of
 * operation: round robin, packet-size based, and fail-over.
 */

#include <stdint.h>
#include "rte_cryptodev_scheduler_operations.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of bonded devices per device */
#ifndef RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES
#define RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES	(8)
#endif

/** Maximum number of multi-core worker cores */
#define RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKER_CORES	(RTE_MAX_LCORE - 1)

/** Round-robin scheduling mode string */
#define SCHEDULER_MODE_NAME_ROUND_ROBIN		round-robin
/** Packet-size based distribution scheduling mode string */
#define SCHEDULER_MODE_NAME_PKT_SIZE_DISTR	packet-size-distr
/** Fail-over scheduling mode string */
#define SCHEDULER_MODE_NAME_FAIL_OVER		fail-over
/** multi-core scheduling mode string */
#define SCHEDULER_MODE_NAME_MULTI_CORE		multi-core

/**
 * Crypto scheduler PMD operation modes
 */
enum rte_cryptodev_scheduler_mode {
	CDEV_SCHED_MODE_NOT_SET = 0,
	/** User defined mode */
	CDEV_SCHED_MODE_USERDEFINED,
	/** Round-robin mode */
	CDEV_SCHED_MODE_ROUNDROBIN,
	/** Packet-size based distribution mode */
	CDEV_SCHED_MODE_PKT_SIZE_DISTR,
	/** Fail-over mode */
	CDEV_SCHED_MODE_FAILOVER,
	/** multi-core mode */
	CDEV_SCHED_MODE_MULTICORE,

	CDEV_SCHED_MODE_COUNT /**< number of modes */
};

#define RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN	(64)
#define RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN	(256)

/**
 * Crypto scheduler option types
 */
enum rte_cryptodev_schedule_option_type {
	CDEV_SCHED_OPTION_NOT_SET = 0,
	CDEV_SCHED_OPTION_THRESHOLD,

	CDEV_SCHED_OPTION_COUNT
};

/**
 * Threshold option structure
 */
struct rte_cryptodev_scheduler_threshold_option {
	uint32_t threshold;	/**< Threshold for packet-size mode */
};

struct rte_cryptodev_scheduler;

/**
 * Load a user defined scheduler
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param scheduler
 *   Pointer to the user defined scheduler
 *
 * @return
 *   - 0 if the scheduler is successfully loaded
 *   - -ENOTSUP if the operation is not supported.
 *   - -EBUSY if device is started.
 *   - -EINVAL if input values are invalid.
 */
int
rte_cryptodev_scheduler_load_user_scheduler(uint8_t scheduler_id,
		struct rte_cryptodev_scheduler *scheduler);

/**
 * Attach a crypto device to the scheduler
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param slave_id
 *   Crypto device ID to be attached
 *
 * @return
 *   - 0 if the slave is attached.
 *   - -ENOTSUP if the operation is not supported.
 *   - -EBUSY if device is started.
 *   - -ENOMEM if the scheduler's slave list is full.
 */
int
rte_cryptodev_scheduler_slave_attach(uint8_t scheduler_id, uint8_t slave_id);

/**
 * Detach a crypto device from the scheduler
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param slave_id
 *   Crypto device ID to be detached
 *
 * @return
 *   - 0 if the slave is detached.
 *   - -ENOTSUP if the operation is not supported.
 *   - -EBUSY if device is started.
 */
int
rte_cryptodev_scheduler_slave_detach(uint8_t scheduler_id, uint8_t slave_id);


/**
 * Set the scheduling mode
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param mode
 *   The scheduling mode
 *
 * @return
 *   - 0 if the mode is set.
 *   - -ENOTSUP if the operation is not supported.
 *   - -EBUSY if device is started.
 */
int
rte_cryptodev_scheduler_mode_set(uint8_t scheduler_id,
		enum rte_cryptodev_scheduler_mode mode);

/**
 * Get the current scheduling mode
 *
 * @param scheduler_id
 *   The target scheduler device ID
 *
 * @return mode
 *   - non-negative enumerate value: the scheduling mode
 *   - -ENOTSUP if the operation is not supported.
 */
enum rte_cryptodev_scheduler_mode
rte_cryptodev_scheduler_mode_get(uint8_t scheduler_id);

/**
 * Set the crypto ops reordering feature on/off
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param enable_reorder
 *   Set the crypto op reordering feature
 *   - 0: disable reordering
 *   - 1: enable reordering
 *
 * @return
 *   - 0 if the ordering is set.
 *   - -ENOTSUP if the operation is not supported.
 *   - -EBUSY if device is started.
 */
int
rte_cryptodev_scheduler_ordering_set(uint8_t scheduler_id,
		uint32_t enable_reorder);

/**
 * Get the current crypto ops reordering feature
 *
 * @param scheduler_id
 *   The target scheduler device ID
 *
 * @return
 *   - 0 if reordering is disabled
 *   - 1 if reordering is enabled
 *   - -ENOTSUP if the operation is not supported.
 */
int
rte_cryptodev_scheduler_ordering_get(uint8_t scheduler_id);

/**
 * Get the attached slaves' count and/or ID
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param slaves
 *   If successful, the function will write back all slaves' device IDs to it.
 *   This parameter will either be an uint8_t array of
 *   RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES elements or NULL.
 *
 * @return
 *   - non-negative number: the number of slaves attached
 *   - -ENOTSUP if the operation is not supported.
 */
int
rte_cryptodev_scheduler_slaves_get(uint8_t scheduler_id, uint8_t *slaves);

/**
 * Set the mode specific option
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param option_type
 *   The option type enumerate
 * @param option
 *   The specific mode's option structure
 *
 * @return
 *   - 0 if successful
 *   - negative integer if otherwise.
 */
int
rte_cryptodev_scheduler_option_set(uint8_t scheduler_id,
		enum rte_cryptodev_schedule_option_type option_type,
		void *option);

/**
 * Set the mode specific option
 *
 * @param scheduler_id
 *   The target scheduler device ID
 * @param option_type
 *   The option type enumerate
 * @param option
 *   If successful, the function will write back the current
 *
 * @return
 *   - 0 if successful
 *   - negative integer if otherwise.
 */
int
rte_cryptodev_scheduler_option_get(uint8_t scheduler_id,
		enum rte_cryptodev_schedule_option_type option_type,
		void *option);

typedef uint16_t (*rte_cryptodev_scheduler_burst_enqueue_t)(void *qp_ctx,
		struct rte_crypto_op **ops, uint16_t nb_ops);

typedef uint16_t (*rte_cryptodev_scheduler_burst_dequeue_t)(void *qp_ctx,
		struct rte_crypto_op **ops, uint16_t nb_ops);

/** The data structure associated with each mode of scheduler. */
struct rte_cryptodev_scheduler {
	const char *name;                        /**< Scheduler name */
	const char *description;                 /**< Scheduler description */
	enum rte_cryptodev_scheduler_mode mode;  /**< Scheduling mode */

	/** Pointer to scheduler operation structure */
	struct rte_cryptodev_scheduler_ops *ops;
};

/** Round-robin mode scheduler */
extern struct rte_cryptodev_scheduler *roundrobin_scheduler;
/** Packet-size based distribution mode scheduler */
extern struct rte_cryptodev_scheduler *pkt_size_based_distr_scheduler;
/** Fail-over mode scheduler */
extern struct rte_cryptodev_scheduler *failover_scheduler;
/** multi-core mode scheduler */
extern struct rte_cryptodev_scheduler *multicore_scheduler;

#ifdef __cplusplus
}
#endif
#endif /* _RTE_CRYPTO_SCHEDULER_H */
