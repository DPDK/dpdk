/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _TEST_PERF_COMMON_
#define _TEST_PERF_COMMON_

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <rte_cryptodev.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_event_crypto_adapter.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_event_timer_adapter.h>
#include <rte_event_vector_adapter.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include "evt_common.h"
#include "evt_options.h"
#include "evt_test.h"

#define TEST_PERF_CA_ID 0
#define TEST_PERF_DA_ID 0

struct test_perf;

struct __rte_cache_aligned worker_data {
	uint64_t processed_pkts;
	uint64_t processed_vecs;
	uint64_t latency;
	uint8_t dev_id;
	uint8_t port_id;
	struct test_perf *t;
};

struct crypto_adptr_data {
	uint8_t cdev_id;
	uint16_t cdev_qp_id;
	void **crypto_sess;
};

struct dma_adptr_data {
	uint8_t dma_dev_id;
	uint16_t vchan_id;
};

struct vector_adptr_data {
	struct rte_event_vector_adapter *vector_adptr;
};

struct __rte_cache_aligned prod_data {
	uint8_t dev_id;
	uint8_t port_id;
	uint8_t queue_id;
	struct crypto_adptr_data ca;
	struct dma_adptr_data da;
	struct vector_adptr_data va;
	struct test_perf *t;
};

struct __rte_cache_aligned test_perf {
	/* Don't change the offset of "done". Signal handler use this memory
	 * to terminate all lcores work.
	 */
	int done;
	uint64_t outstand_pkts;
	uint8_t nb_workers;
	enum evt_test_result result;
	uint32_t nb_flows;
	uint64_t nb_pkts;
	struct rte_mempool *pool;
	struct prod_data prod[EVT_MAX_PORTS];
	struct worker_data worker[EVT_MAX_PORTS];
	struct evt_options *opt;
	alignas(RTE_CACHE_LINE_SIZE) uint8_t sched_type_list[EVT_MAX_STAGES];
	alignas(RTE_CACHE_LINE_SIZE) struct rte_event_timer_adapter *timer_adptr[
		RTE_EVENT_TIMER_ADAPTER_NUM_MAX];
	struct rte_mempool *ca_op_pool;
	struct rte_mempool *ca_sess_pool;
	struct rte_mempool *ca_asym_sess_pool;
	struct rte_mempool *ca_vector_pool;
};

struct __rte_cache_aligned perf_elt {
	union {
		struct rte_event_timer tim;
		struct {
			char pad[offsetof(struct rte_event_timer, user_meta)];
			uint64_t timestamp;
		};
	};
};

#define BURST_SIZE 16
#define MAX_PROD_ENQ_BURST_SIZE 128

#define PERF_WORKER_INIT\
	struct worker_data *w  = arg;\
	struct test_perf *t = w->t;\
	struct evt_options *opt = t->opt;\
	const uint8_t dev = w->dev_id;\
	const uint8_t port = w->port_id;\
	const uint8_t prod_timer_type = \
		opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR;\
	uint8_t *const sched_type_list = &t->sched_type_list[0];\
	const enum evt_prod_type prod_type = opt->prod_type;\
	struct rte_mempool *const pool = t->pool;\
	const uint8_t nb_stages = t->opt->nb_stages;\
	const uint8_t laststage = nb_stages - 1;\
	uint8_t cnt = 0;\
	alignas(RTE_CACHE_LINE_SIZE) void *bufs[16];\
	int const sz = RTE_DIM(bufs);\
	uint8_t stage;\
	struct perf_elt *pe = NULL;\
	if (opt->verbose_level > 1)\
		printf("%s(): lcore %d dev_id %d port=%d\n", __func__,\
				rte_lcore_id(), dev, port)

static __rte_always_inline void
perf_mark_fwd_latency(enum evt_prod_type prod_type, struct rte_event *const ev)
{
	struct perf_elt *pe;

	if (prod_type == EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR) {
		struct rte_crypto_op *op = ev->event_ptr;
		struct rte_mbuf *m;

		if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (op->sym->m_dst == NULL)
				m = op->sym->m_src;
			else
				m = op->sym->m_dst;

			pe = rte_pktmbuf_mtod(m, struct perf_elt *);
		} else {
			pe = RTE_PTR_ADD(op->asym->modex.result.data,
					 op->asym->modex.result.length);
		}
		pe->timestamp = rte_get_timer_cycles();
	} else if (prod_type == EVT_PROD_TYPE_EVENT_DMA_ADPTR) {
		struct rte_event_dma_adapter_op *op = ev->event_ptr;

		op->user_meta = rte_get_timer_cycles();
	} else {
		pe = ev->event_ptr;
		pe->timestamp = rte_get_timer_cycles();
	}
}

static __rte_always_inline int
perf_handle_crypto_ev(struct rte_event *ev)
{
	struct rte_crypto_op *op = ev->event_ptr;

	if (unlikely(op->status != RTE_CRYPTO_OP_STATUS_SUCCESS)) {
		rte_crypto_op_free(op);
		return op->status;
	}

	return 0;
}

static __rte_always_inline struct perf_elt *
perf_elt_from_vec_get(struct rte_event_vector *vec)
{
	/* Timestamp for vector event stored in first element */
	struct rte_crypto_op *cop = vec->ptrs[0];
	struct rte_mbuf *m;

	if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
		m = cop->sym->m_dst == NULL ? cop->sym->m_src : cop->sym->m_dst;
		return rte_pktmbuf_mtod(m, struct perf_elt *);
	} else {
		return RTE_PTR_ADD(cop->asym->modex.result.data, cop->asym->modex.result.length);
	}
}

static __rte_always_inline int
perf_handle_crypto_vector_ev(struct rte_event *ev, struct perf_elt **pe,
		const int enable_fwd_latency)
{
	struct rte_event_vector *vec = ev->vec;
	struct rte_crypto_op *cop;
	struct rte_mbuf *m;
	int i, n = 0;
	void *data;

	for (i = 0; i < vec->nb_elem; i++) {
		cop = vec->ptrs[i];
		if (unlikely(cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS)) {
			if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
				m = cop->sym->m_dst == NULL ? cop->sym->m_src : cop->sym->m_dst;
				rte_pktmbuf_free(m);
			} else {
				data = cop->asym->modex.result.data;
				rte_mempool_put(rte_mempool_from_obj(data), data);
			}
			rte_crypto_op_free(cop);
			continue;
		}
		vec->ptrs[n++] = cop;
	}

	/* All cops failed, free the vector */
	if (n == 0) {
		rte_mempool_put(rte_mempool_from_obj(vec), vec);
		return -ENOENT;
	}

	vec->nb_elem = n;

	/* Forward latency not enabled - perf data will be not accessed */
	if (!enable_fwd_latency)
		return 0;

	/* Get pointer to perf data */
	*pe = perf_elt_from_vec_get(vec);

	return 0;
}

static __rte_always_inline int
perf_process_last_stage(struct rte_mempool *const pool, enum evt_prod_type prod_type,
			struct rte_event *const ev, struct worker_data *const w,
			void *bufs[], int const buf_sz, uint8_t count)
{
	void *to_free_in_bulk;

	/* release fence here ensures event_prt is
	 * stored before updating the number of
	 * processed packets for worker lcores
	 */
	rte_atomic_thread_fence(rte_memory_order_release);
	w->processed_pkts++;

	if (prod_type == EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR) {
		struct rte_crypto_op *op = ev->event_ptr;
		struct rte_mbuf *m;

		if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (op->sym->m_dst == NULL)
				m = op->sym->m_src;
			else
				m = op->sym->m_dst;

			to_free_in_bulk = m;
		} else {
			to_free_in_bulk = op->asym->modex.result.data;
		}
		rte_crypto_op_free(op);
	} else {
		to_free_in_bulk = ev->event_ptr;
	}

	bufs[count++] = to_free_in_bulk;
	if (unlikely(count == buf_sz)) {
		count = 0;
		rte_mempool_put_bulk(pool, bufs, buf_sz);
	}

	return count;
}

static __rte_always_inline uint8_t
perf_process_last_stage_latency(struct rte_mempool *const pool, enum evt_prod_type prod_type,
				struct rte_event *const ev, struct worker_data *const w,
				void *bufs[], int const buf_sz, uint8_t count)
{
	uint64_t latency, tstamp;
	struct perf_elt *pe;
	void *to_free_in_bulk;

	/* Release fence here ensures event_prt is stored before updating the number of processed
	 * packets for worker lcores.
	 */
	rte_atomic_thread_fence(rte_memory_order_release);
	w->processed_pkts++;

	if (prod_type == EVT_PROD_TYPE_EVENT_CRYPTO_ADPTR) {
		struct rte_crypto_op *op = ev->event_ptr;
		struct rte_mbuf *m;

		if (op->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
			if (op->sym->m_dst == NULL)
				m = op->sym->m_src;
			else
				m = op->sym->m_dst;

			to_free_in_bulk = m;
			pe = rte_pktmbuf_mtod(m, struct perf_elt *);
		} else {
			pe = RTE_PTR_ADD(op->asym->modex.result.data,
					 op->asym->modex.result.length);
			to_free_in_bulk = op->asym->modex.result.data;
		}
		tstamp = pe->timestamp;
		rte_crypto_op_free(op);
	} else if (prod_type == EVT_PROD_TYPE_EVENT_DMA_ADPTR) {
		struct rte_event_dma_adapter_op *op = ev->event_ptr;

		to_free_in_bulk = op;
		tstamp = op->user_meta;
	} else {
		pe = ev->event_ptr;
		tstamp = pe->timestamp;
		to_free_in_bulk = pe;
	}

	latency = rte_get_timer_cycles() - tstamp;
	w->latency += latency;

	bufs[count++] = to_free_in_bulk;
	if (unlikely(count == buf_sz)) {
		count = 0;
		rte_mempool_put_bulk(pool, bufs, buf_sz);
	}

	return count;
}

static __rte_always_inline void
perf_process_crypto_vector_last_stage(struct rte_mempool *const pool,
				      struct rte_mempool *const ca_pool, struct rte_event *const ev,
				      struct worker_data *const w, const bool enable_fwd_latency)
{
	struct rte_event_vector *vec = ev->vec;
	struct rte_crypto_op *cop;
	void *bufs[vec->nb_elem];
	struct perf_elt *pe;
	uint64_t latency;
	int i;

	/* Release fence here ensures event_prt is stored before updating the number of processed
	 * packets for worker lcores.
	 */
	rte_atomic_thread_fence(rte_memory_order_release);
	w->processed_pkts += vec->nb_elem;

	if (enable_fwd_latency) {
		pe = perf_elt_from_vec_get(vec);
		latency = rte_get_timer_cycles() - pe->timestamp;
		w->latency += latency;
	}

	for (i = 0; i < vec->nb_elem; i++) {
		cop = vec->ptrs[i];
		if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC)
			bufs[i] = cop->sym->m_dst == NULL ? cop->sym->m_src : cop->sym->m_dst;
		else
			bufs[i] = cop->asym->modex.result.data;
	}

	rte_mempool_put_bulk(pool, bufs, vec->nb_elem);
	rte_mempool_put_bulk(ca_pool, (void * const *)vec->ptrs, vec->nb_elem);
	rte_mempool_put(rte_mempool_from_obj(vec), vec);
}

static inline int
perf_nb_event_ports(struct evt_options *opt)
{
	return evt_nr_active_lcores(opt->wlcores) +
			evt_nr_active_lcores(opt->plcores);
}

int perf_test_result(struct evt_test *test, struct evt_options *opt);
int perf_opt_check(struct evt_options *opt, uint64_t nb_queues);
int perf_test_setup(struct evt_test *test, struct evt_options *opt);
int perf_ethdev_setup(struct evt_test *test, struct evt_options *opt);
int perf_cryptodev_setup(struct evt_test *test, struct evt_options *opt);
int perf_dmadev_setup(struct evt_test *test, struct evt_options *opt);
int perf_mempool_setup(struct evt_test *test, struct evt_options *opt);
int perf_event_dev_port_setup(struct evt_test *test, struct evt_options *opt,
				uint8_t stride, uint8_t nb_queues,
				const struct rte_event_port_conf *port_conf);
int perf_event_dev_service_setup(uint8_t dev_id);
int perf_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *));
void perf_opt_dump(struct evt_options *opt, uint8_t nb_queues);
void perf_test_destroy(struct evt_test *test, struct evt_options *opt);
void perf_eventdev_destroy(struct evt_test *test, struct evt_options *opt);
void perf_cryptodev_destroy(struct evt_test *test, struct evt_options *opt);
void perf_dmadev_destroy(struct evt_test *test, struct evt_options *opt);
void perf_ethdev_destroy(struct evt_test *test, struct evt_options *opt);
void perf_ethdev_rx_stop(struct evt_test *test, struct evt_options *opt);
void perf_mempool_destroy(struct evt_test *test, struct evt_options *opt);
void perf_worker_cleanup(struct rte_mempool *const pool, uint8_t dev_id,
			 uint8_t port_id, struct rte_event events[],
			 uint16_t nb_enq, uint16_t nb_deq);

#endif /* _TEST_PERF_COMMON_ */
