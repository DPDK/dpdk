/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _SCHEDULER_PMD_PRIVATE_H
#define _SCHEDULER_PMD_PRIVATE_H

#include "rte_cryptodev_scheduler.h"

#define CRYPTODEV_NAME_SCHEDULER_PMD	crypto_scheduler
/**< Scheduler Crypto PMD device name */

#define PER_WORKER_BUFF_SIZE			(256)

extern int scheduler_logtype_driver;

#define CR_SCHED_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, scheduler_logtype_driver,		\
			"%s() line %u: "fmt "\n", __func__, __LINE__, ##args)

struct scheduler_worker {
	uint8_t dev_id;
	uint16_t qp_id;
	uint32_t nb_inflight_cops;

	uint8_t driver_id;
};

struct scheduler_ctx {
	void *private_ctx;
	/**< private scheduler context pointer */

	struct rte_cryptodev_capabilities *capabilities;
	uint32_t nb_capabilities;

	uint32_t max_nb_queue_pairs;

	struct scheduler_worker workers[RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
	uint32_t nb_workers;

	enum rte_cryptodev_scheduler_mode mode;

	struct rte_cryptodev_scheduler_ops ops;

	uint8_t reordering_enabled;

	char name[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN];
	char description[RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN];
	uint16_t wc_pool[RTE_MAX_LCORE];
	uint16_t nb_wc;

	char *init_worker_names[RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
	int nb_init_workers;
} __rte_cache_aligned;

struct scheduler_qp_ctx {
	void *private_qp_ctx;

	uint32_t max_nb_objs;

	struct rte_ring *order_ring;
} __rte_cache_aligned;


extern uint8_t cryptodev_scheduler_driver_id;

static __rte_always_inline uint16_t
get_max_enqueue_order_count(struct rte_ring *order_ring, uint16_t nb_ops)
{
	uint32_t count = rte_ring_free_count(order_ring);

	return count > nb_ops ? nb_ops : count;
}

static __rte_always_inline void
scheduler_order_insert(struct rte_ring *order_ring,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	rte_ring_sp_enqueue_burst(order_ring, (void **)ops, nb_ops, NULL);
}

static __rte_always_inline uint16_t
scheduler_order_drain(struct rte_ring *order_ring,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_crypto_op *op;
	uint32_t nb_objs, nb_ops_to_deq;

	nb_objs = rte_ring_dequeue_burst_start(order_ring, (void **)ops,
		nb_ops, NULL);
	if (nb_objs == 0)
		return 0;

	for (nb_ops_to_deq = 0; nb_ops_to_deq != nb_objs; nb_ops_to_deq++) {
		op = ops[nb_ops_to_deq];
		if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			break;
	}

	rte_ring_dequeue_finish(order_ring, nb_ops_to_deq);
	return nb_ops_to_deq;
}

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops;

#endif /* _SCHEDULER_PMD_PRIVATE_H */
