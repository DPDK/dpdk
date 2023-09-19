/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _SCHEDULER_PMD_PRIVATE_H
#define _SCHEDULER_PMD_PRIVATE_H

#include <rte_security_driver.h>

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
	struct rte_security_capability *sec_capabilities;
	struct rte_cryptodev_capabilities **sec_crypto_capabilities;

	uint32_t max_nb_queue_pairs;

	struct scheduler_worker workers[RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
	uint32_t nb_workers;
	/* reference count when the workers are incremented/decremented */
	uint32_t ref_cnt;

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

struct scheduler_session_ctx {
	uint32_t ref_cnt;
	union {
		struct rte_cryptodev_sym_session *worker_sess[
			RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
		struct rte_security_session *worker_sec_sess[
			RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
	};
};

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

static __rte_always_inline void
scheduler_set_single_worker_session(struct rte_crypto_op *op,
		uint8_t worker_idx)
{
	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION) {
		struct scheduler_session_ctx *sess_ctx =
				CRYPTODEV_GET_SYM_SESS_PRIV(op->sym->session);
		op->sym->session = sess_ctx->worker_sess[worker_idx];
	} else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION) {
		struct scheduler_session_ctx *sess_ctx =
				SECURITY_GET_SESS_PRIV(op->sym->session);
		op->sym->session = sess_ctx->worker_sec_sess[worker_idx];
	}
}

static __rte_always_inline void
scheduler_set_worker_sessions(struct rte_crypto_op **ops, uint16_t nb_ops,
		uint8_t worker_index)
{
	struct rte_crypto_op **op = ops;
	uint16_t n = nb_ops;

	if (n >= 4) {
		rte_prefetch0(op[0]->sym->session);
		rte_prefetch0(op[1]->sym->session);
		rte_prefetch0(op[2]->sym->session);
		rte_prefetch0(op[3]->sym->session);
	}

	while (n >= 4) {
		if (n >= 8) {
			rte_prefetch0(op[4]->sym->session);
			rte_prefetch0(op[5]->sym->session);
			rte_prefetch0(op[6]->sym->session);
			rte_prefetch0(op[7]->sym->session);
		}

		scheduler_set_single_worker_session(op[0], worker_index);
		scheduler_set_single_worker_session(op[1], worker_index);
		scheduler_set_single_worker_session(op[2], worker_index);
		scheduler_set_single_worker_session(op[3], worker_index);

		op += 4;
		n -= 4;
	}

	while (n--) {
		scheduler_set_single_worker_session(op[0], worker_index);
		op++;
	}
}

static __rte_always_inline void
scheduler_retrieve_single_session(struct rte_crypto_op *op)
{
	if (op->sess_type == RTE_CRYPTO_OP_WITH_SESSION)
		op->sym->session = (void *)(uintptr_t)
			rte_cryptodev_sym_session_opaque_data_get(op->sym->session);
	else if (op->sess_type == RTE_CRYPTO_OP_SECURITY_SESSION)
		op->sym->session = (void *)(uintptr_t)
			rte_security_session_opaque_data_get(op->sym->session);
}

static __rte_always_inline void
scheduler_retrieve_sessions(struct rte_crypto_op **ops, uint16_t nb_ops)
{
	uint16_t n = nb_ops;
	struct rte_crypto_op **op = ops;

	if (n >= 4) {
		rte_prefetch0(op[0]->sym->session);
		rte_prefetch0(op[1]->sym->session);
		rte_prefetch0(op[2]->sym->session);
		rte_prefetch0(op[3]->sym->session);
	}

	while (n >= 4) {
		if (n >= 8) {
			rte_prefetch0(op[4]->sym->session);
			rte_prefetch0(op[5]->sym->session);
			rte_prefetch0(op[6]->sym->session);
			rte_prefetch0(op[7]->sym->session);
		}

		scheduler_retrieve_single_session(op[0]);
		scheduler_retrieve_single_session(op[1]);
		scheduler_retrieve_single_session(op[2]);
		scheduler_retrieve_single_session(op[3]);

		op += 4;
		n -= 4;
	}

	while (n--) {
		scheduler_retrieve_single_session(op[0]);
		op++;
	}
}

static __rte_always_inline uint32_t
scheduler_get_job_len(struct rte_crypto_op *op)
{
	uint32_t job_len;

	/* op_len is initialized as cipher data length, if
	 * it is 0, then it is set to auth data length
	 */
	job_len = op->sym->cipher.data.length;
	job_len += (op->sym->cipher.data.length == 0) *
					op->sym->auth.data.length;

	return job_len;
}

static __rte_always_inline void
scheduler_free_capabilities(struct scheduler_ctx *sched_ctx)
{
	uint32_t i;

	rte_free(sched_ctx->capabilities);
	sched_ctx->capabilities = NULL;

	if (sched_ctx->sec_crypto_capabilities) {
		i = 0;
		while (sched_ctx->sec_crypto_capabilities[i] != NULL) {
			rte_free(sched_ctx->sec_crypto_capabilities[i]);
			sched_ctx->sec_crypto_capabilities[i] = NULL;
			i++;
		}

		rte_free(sched_ctx->sec_crypto_capabilities);
		sched_ctx->sec_crypto_capabilities = NULL;
	}

	rte_free(sched_ctx->sec_capabilities);
	sched_ctx->sec_capabilities = NULL;
}

static __rte_always_inline int
scheduler_check_sec_proto_supp(enum rte_security_session_action_type action,
		enum rte_security_session_protocol protocol)
{
	if (action == RTE_SECURITY_ACTION_TYPE_LOOKASIDE_PROTOCOL &&
			protocol == RTE_SECURITY_PROTOCOL_DOCSIS)
		return 1;

	return 0;
}

/** device specific operations function pointer structure */
extern struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops;
extern struct rte_security_ops *rte_crypto_scheduler_pmd_sec_ops;

#endif /* _SCHEDULER_PMD_PRIVATE_H */
