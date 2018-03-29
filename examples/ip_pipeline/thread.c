/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_ring.h>

#include <rte_table_acl.h>
#include <rte_table_array.h>
#include <rte_table_hash.h>
#include <rte_table_lpm.h>
#include <rte_table_lpm_ipv6.h>

#include "common.h"
#include "thread.h"
#include "pipeline.h"

#ifndef THREAD_PIPELINES_MAX
#define THREAD_PIPELINES_MAX                               256
#endif

#ifndef THREAD_MSGQ_SIZE
#define THREAD_MSGQ_SIZE                                   64
#endif

#ifndef THREAD_TIMER_PERIOD_MS
#define THREAD_TIMER_PERIOD_MS                             100
#endif

/**
 * Master thead: data plane thread context
 */
struct thread {
	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;

	uint32_t enabled;
};

static struct thread thread[RTE_MAX_LCORE];

/**
 * Data plane threads: context
 */
struct table_data {
	struct rte_table_action *a;
};

struct pipeline_data {
	struct rte_pipeline *p;
	struct table_data table_data[RTE_PIPELINE_TABLE_MAX];
	uint32_t n_tables;

	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;
	uint64_t timer_period; /* Measured in CPU cycles. */
	uint64_t time_next;

	uint8_t buffer[TABLE_RULE_ACTION_SIZE_MAX];
};

struct thread_data {
	struct rte_pipeline *p[THREAD_PIPELINES_MAX];
	uint32_t n_pipelines;

	struct pipeline_data pipeline_data[THREAD_PIPELINES_MAX];
	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;
	uint64_t timer_period; /* Measured in CPU cycles. */
	uint64_t time_next;
	uint64_t time_next_min;
} __rte_cache_aligned;

static struct thread_data thread_data[RTE_MAX_LCORE];

/**
 * Master thread: data plane thread init
 */
static void
thread_free(void)
{
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct thread *t = &thread[i];

		if (!rte_lcore_is_enabled(i))
			continue;

		/* MSGQs */
		if (t->msgq_req)
			rte_ring_free(t->msgq_req);

		if (t->msgq_rsp)
			rte_ring_free(t->msgq_rsp);
	}
}

int
thread_init(void)
{
	uint32_t i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		char name[NAME_MAX];
		struct rte_ring *msgq_req, *msgq_rsp;
		struct thread *t = &thread[i];
		struct thread_data *t_data = &thread_data[i];
		uint32_t cpu_id = rte_lcore_to_socket_id(i);

		/* MSGQs */
		snprintf(name, sizeof(name), "THREAD-%04x-MSGQ-REQ", i);

		msgq_req = rte_ring_create(name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_req == NULL) {
			thread_free();
			return -1;
		}

		snprintf(name, sizeof(name), "THREAD-%04x-MSGQ-RSP", i);

		msgq_rsp = rte_ring_create(name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_rsp == NULL) {
			thread_free();
			return -1;
		}

		/* Master thread records */
		t->msgq_req = msgq_req;
		t->msgq_rsp = msgq_rsp;
		t->enabled = 1;

		/* Data plane thread records */
		t_data->n_pipelines = 0;
		t_data->msgq_req = msgq_req;
		t_data->msgq_rsp = msgq_rsp;
		t_data->timer_period =
			(rte_get_tsc_hz() * THREAD_TIMER_PERIOD_MS) / 1000;
		t_data->time_next = rte_get_tsc_cycles() + t_data->timer_period;
		t_data->time_next_min = t_data->time_next;
	}

	return 0;
}

/**
 * Master thread & data plane threads: message passing
 */
enum thread_req_type {
	THREAD_REQ_MAX
};

struct thread_msg_req {
	enum thread_req_type type;
};

struct thread_msg_rsp {
	int status;
};

/**
 * Data plane threads: message handling
 */
static inline struct thread_msg_req *
thread_msg_recv(struct rte_ring *msgq_req)
{
	struct thread_msg_req *req;

	int status = rte_ring_sc_dequeue(msgq_req, (void **) &req);

	if (status != 0)
		return NULL;

	return req;
}

static inline void
thread_msg_send(struct rte_ring *msgq_rsp,
	struct thread_msg_rsp *rsp)
{
	int status;

	do {
		status = rte_ring_sp_enqueue(msgq_rsp, rsp);
	} while (status == -ENOBUFS);
}

static void
thread_msg_handle(struct thread_data *t)
{
	for ( ; ; ) {
		struct thread_msg_req *req;
		struct thread_msg_rsp *rsp;

		req = thread_msg_recv(t->msgq_req);
		if (req == NULL)
			break;

		switch (req->type) {
		default:
			rsp = (struct thread_msg_rsp *) req;
			rsp->status = -1;
		}

		thread_msg_send(t->msgq_rsp, rsp);
	}
}

/**
 * Master thread & data plane threads: message passing
 */

enum pipeline_req_type {
	PIPELINE_REQ_MAX
};

struct pipeline_msg_req {
	enum pipeline_req_type type;
};

struct pipeline_msg_rsp {
	int status;
};

/**
 * Data plane threads: message handling
 */
static inline struct pipeline_msg_req *
pipeline_msg_recv(struct rte_ring *msgq_req)
{
	struct pipeline_msg_req *req;

	int status = rte_ring_sc_dequeue(msgq_req, (void **) &req);

	if (status != 0)
		return NULL;

	return req;
}

static inline void
pipeline_msg_send(struct rte_ring *msgq_rsp,
	struct pipeline_msg_rsp *rsp)
{
	int status;

	do {
		status = rte_ring_sp_enqueue(msgq_rsp, rsp);
	} while (status == -ENOBUFS);
}

static void
pipeline_msg_handle(struct pipeline_data *p)
{
	for ( ; ; ) {
		struct pipeline_msg_req *req;
		struct pipeline_msg_rsp *rsp;

		req = pipeline_msg_recv(p->msgq_req);
		if (req == NULL)
			break;

		switch (req->type) {
		default:
			rsp = (struct pipeline_msg_rsp *) req;
			rsp->status = -1;
		}

		pipeline_msg_send(p->msgq_rsp, rsp);
	}
}

/**
 * Data plane threads: main
 */
int
thread_main(void *arg __rte_unused)
{
	struct thread_data *t;
	uint32_t thread_id, i;

	thread_id = rte_lcore_id();
	t = &thread_data[thread_id];

	/* Dispatch loop */
	for (i = 0; ; i++) {
		uint32_t j;

		/* Data Plane */
		for (j = 0; j < t->n_pipelines; j++)
			rte_pipeline_run(t->p[j]);

		/* Control Plane */
		if ((i & 0xF) == 0) {
			uint64_t time = rte_get_tsc_cycles();
			uint64_t time_next_min = UINT64_MAX;

			if (time < t->time_next_min)
				continue;

			/* Pipeline message queues */
			for (j = 0; j < t->n_pipelines; j++) {
				struct pipeline_data *p =
					&t->pipeline_data[j];
				uint64_t time_next = p->time_next;

				if (time_next <= time) {
					pipeline_msg_handle(p);
					rte_pipeline_flush(p->p);
					time_next = time + p->timer_period;
					p->time_next = time_next;
				}

				if (time_next < time_next_min)
					time_next_min = time_next;
			}

			/* Thread message queues */
			{
				uint64_t time_next = t->time_next;

				if (time_next <= time) {
					thread_msg_handle(t);
					time_next = time + t->timer_period;
					t->time_next = time_next;
				}

				if (time_next < time_next_min)
					time_next_min = time_next;
			}

			t->time_next_min = time_next_min;
		}
	}

	return 0;
}
