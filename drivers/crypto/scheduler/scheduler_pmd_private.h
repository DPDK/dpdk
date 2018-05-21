/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _SCHEDULER_PMD_PRIVATE_H
#define _SCHEDULER_PMD_PRIVATE_H

#include "rte_cryptodev_scheduler.h"

#define CRYPTODEV_NAME_SCHEDULER_PMD	crypto_scheduler
/**< Scheduler Crypto PMD device name */

#define PER_SLAVE_BUFF_SIZE			(256)

#define CS_LOG_ERR(fmt, args...)					\
	RTE_LOG(ERR, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",		\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)

#ifdef RTE_LIBRTE_CRYPTO_SCHEDULER_DEBUG
#define CS_LOG_INFO(fmt, args...)					\
	RTE_LOG(INFO, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",	\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)

#define CS_LOG_DBG(fmt, args...)					\
	RTE_LOG(DEBUG, CRYPTODEV, "[%s] %s() line %u: " fmt "\n",	\
		RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),			\
		__func__, __LINE__, ## args)
#else
#define CS_LOG_INFO(fmt, args...)
#define CS_LOG_DBG(fmt, args...)
#endif

struct scheduler_slave {
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

	struct scheduler_slave slaves[RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES];
	uint32_t nb_slaves;

	enum rte_cryptodev_scheduler_mode mode;

	struct rte_cryptodev_scheduler_ops ops;

	uint8_t reordering_enabled;

	char name[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN];
	char description[RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN];
	uint16_t wc_pool[RTE_MAX_LCORE];
	uint16_t nb_wc;

	char *init_slave_names[RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES];
	int nb_init_slaves;
} __rte_cache_aligned;

struct scheduler_qp_ctx {
	void *private_qp_ctx;

	uint32_t max_nb_objs;

	struct rte_ring *order_ring;
	uint32_t seqn;
} __rte_cache_aligned;


extern uint8_t cryptodev_driver_id;

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

#define SCHEDULER_GET_RING_OBJ(order_ring, pos, op) do {            \
	struct rte_crypto_op **ring = (void *)&order_ring[1];     \
	op = ring[(order_ring->cons.head + pos) & order_ring->mask]; \
} while (0)

static __rte_always_inline uint16_t
scheduler_order_drain(struct rte_ring *order_ring,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rte_crypto_op *op;
	uint32_t nb_objs = rte_ring_count(order_ring);
	uint32_t nb_ops_to_deq = 0;
	uint32_t nb_ops_deqd = 0;

	if (nb_objs > nb_ops)
		nb_objs = nb_ops;

	while (nb_ops_to_deq < nb_objs) {
		SCHEDULER_GET_RING_OBJ(order_ring, nb_ops_to_deq, op);
		if (op->status == RTE_CRYPTO_OP_STATUS_NOT_PROCESSED)
			break;
		nb_ops_to_deq++;
	}

	if (nb_ops_to_deq)
		nb_ops_deqd = rte_ring_sc_dequeue_bulk(order_ring,
				(void **)ops, nb_ops_to_deq, NULL);

	return nb_ops_deqd;
}
/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops;

#endif /* _SCHEDULER_PMD_PRIVATE_H */
