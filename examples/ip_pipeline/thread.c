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
	THREAD_REQ_PIPELINE_ENABLE = 0,
	THREAD_REQ_PIPELINE_DISABLE,
	THREAD_REQ_MAX
};

struct thread_msg_req {
	enum thread_req_type type;

	union {
		struct {
			struct rte_pipeline *p;
			struct {
				struct rte_table_action *a;
			} table[RTE_PIPELINE_TABLE_MAX];
			struct rte_ring *msgq_req;
			struct rte_ring *msgq_rsp;
			uint32_t timer_period_ms;
			uint32_t n_tables;
		} pipeline_enable;

		struct {
			struct rte_pipeline *p;
		} pipeline_disable;
	};
};

struct thread_msg_rsp {
	int status;
};

/**
 * Master thread
 */
static struct thread_msg_req *
thread_msg_alloc(void)
{
	size_t size = RTE_MAX(sizeof(struct thread_msg_req),
		sizeof(struct thread_msg_rsp));

	return calloc(1, size);
}

static void
thread_msg_free(struct thread_msg_rsp *rsp)
{
	free(rsp);
}

static struct thread_msg_rsp *
thread_msg_send_recv(uint32_t thread_id,
	struct thread_msg_req *req)
{
	struct thread *t = &thread[thread_id];
	struct rte_ring *msgq_req = t->msgq_req;
	struct rte_ring *msgq_rsp = t->msgq_rsp;
	struct thread_msg_rsp *rsp;
	int status;

	/* send */
	do {
		status = rte_ring_sp_enqueue(msgq_req, req);
	} while (status == -ENOBUFS);

	/* recv */
	do {
		status = rte_ring_sc_dequeue(msgq_rsp, (void **) &rsp);
	} while (status != 0);

	return rsp;
}

int
thread_pipeline_enable(uint32_t thread_id,
	const char *pipeline_name)
{
	struct pipeline *p = pipeline_find(pipeline_name);
	struct thread *t;
	struct thread_msg_req *req;
	struct thread_msg_rsp *rsp;
	uint32_t i;
	int status;

	/* Check input params */
	if ((thread_id >= RTE_MAX_LCORE) ||
		(p == NULL) ||
		(p->n_ports_in == 0) ||
		(p->n_ports_out == 0) ||
		(p->n_tables == 0))
		return -1;

	t = &thread[thread_id];
	if ((t->enabled == 0) ||
		p->enabled)
		return -1;

	/* Allocate request */
	req = thread_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = THREAD_REQ_PIPELINE_ENABLE;
	req->pipeline_enable.p = p->p;
	for (i = 0; i < p->n_tables; i++)
		req->pipeline_enable.table[i].a =
			p->table[i].a;
	req->pipeline_enable.msgq_req = p->msgq_req;
	req->pipeline_enable.msgq_rsp = p->msgq_rsp;
	req->pipeline_enable.timer_period_ms = p->timer_period_ms;
	req->pipeline_enable.n_tables = p->n_tables;

	/* Send request and wait for response */
	rsp = thread_msg_send_recv(thread_id, req);
	if (rsp == NULL)
		return -1;

	/* Read response */
	status = rsp->status;

	/* Free response */
	thread_msg_free(rsp);

	/* Request completion */
	if (status)
		return status;

	p->thread_id = thread_id;
	p->enabled = 1;

	return 0;
}

int
thread_pipeline_disable(uint32_t thread_id,
	const char *pipeline_name)
{
	struct pipeline *p = pipeline_find(pipeline_name);
	struct thread *t;
	struct thread_msg_req *req;
	struct thread_msg_rsp *rsp;
	int status;

	/* Check input params */
	if ((thread_id >= RTE_MAX_LCORE) ||
		(p == NULL))
		return -1;

	t = &thread[thread_id];
	if (t->enabled == 0)
		return -1;

	if (p->enabled == 0)
		return 0;

	if (p->thread_id != thread_id)
		return -1;

	/* Allocate request */
	req = thread_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = THREAD_REQ_PIPELINE_DISABLE;
	req->pipeline_disable.p = p->p;

	/* Send request and wait for response */
	rsp = thread_msg_send_recv(thread_id, req);
	if (rsp == NULL)
		return -1;

	/* Read response */
	status = rsp->status;

	/* Free response */
	thread_msg_free(rsp);

	/* Request completion */
	if (status)
		return status;

	p->enabled = 0;

	return 0;
}

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

static struct thread_msg_rsp *
thread_msg_handle_pipeline_enable(struct thread_data *t,
	struct thread_msg_req *req)
{
	struct thread_msg_rsp *rsp = (struct thread_msg_rsp *) req;
	struct pipeline_data *p = &t->pipeline_data[t->n_pipelines];
	uint32_t i;

	/* Request */
	if (t->n_pipelines >= THREAD_PIPELINES_MAX) {
		rsp->status = -1;
		return rsp;
	}

	t->p[t->n_pipelines] = req->pipeline_enable.p;

	p->p = req->pipeline_enable.p;
	for (i = 0; i < req->pipeline_enable.n_tables; i++)
		p->table_data[i].a =
			req->pipeline_enable.table[i].a;

	p->n_tables = req->pipeline_enable.n_tables;

	p->msgq_req = req->pipeline_enable.msgq_req;
	p->msgq_rsp = req->pipeline_enable.msgq_rsp;
	p->timer_period =
		(rte_get_tsc_hz() * req->pipeline_enable.timer_period_ms) / 1000;
	p->time_next = rte_get_tsc_cycles() + p->timer_period;

	t->n_pipelines++;

	/* Response */
	rsp->status = 0;
	return rsp;
}

static struct thread_msg_rsp *
thread_msg_handle_pipeline_disable(struct thread_data *t,
	struct thread_msg_req *req)
{
	struct thread_msg_rsp *rsp = (struct thread_msg_rsp *) req;
	uint32_t n_pipelines = t->n_pipelines;
	struct rte_pipeline *pipeline = req->pipeline_disable.p;
	uint32_t i;

	/* find pipeline */
	for (i = 0; i < n_pipelines; i++) {
		struct pipeline_data *p = &t->pipeline_data[i];

		if (p->p != pipeline)
			continue;

		if (i < n_pipelines - 1) {
			struct rte_pipeline *pipeline_last =
				t->p[n_pipelines - 1];
			struct pipeline_data *p_last =
				&t->pipeline_data[n_pipelines - 1];

			t->p[i] = pipeline_last;
			memcpy(p, p_last, sizeof(*p));
		}

		t->n_pipelines--;

		rsp->status = 0;
		return rsp;
	}

	/* should not get here */
	rsp->status = 0;
	return rsp;
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
		case THREAD_REQ_PIPELINE_ENABLE:
			rsp = thread_msg_handle_pipeline_enable(t, req);
			break;

		case THREAD_REQ_PIPELINE_DISABLE:
			rsp = thread_msg_handle_pipeline_disable(t, req);
			break;

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
	/* Port IN */
	PIPELINE_REQ_PORT_IN_ENABLE,
	PIPELINE_REQ_PORT_IN_DISABLE,

	PIPELINE_REQ_MAX
};

struct pipeline_msg_req {
	enum pipeline_req_type type;
	uint32_t id; /* Port IN, port OUT or table ID */
};

struct pipeline_msg_rsp {
	int status;
};

/**
 * Master thread
 */
static struct pipeline_msg_req *
pipeline_msg_alloc(void)
{
	size_t size = RTE_MAX(sizeof(struct pipeline_msg_req),
		sizeof(struct pipeline_msg_rsp));

	return calloc(1, size);
}

static void
pipeline_msg_free(struct pipeline_msg_rsp *rsp)
{
	free(rsp);
}

static struct pipeline_msg_rsp *
pipeline_msg_send_recv(struct pipeline *p,
	struct pipeline_msg_req *req)
{
	struct rte_ring *msgq_req = p->msgq_req;
	struct rte_ring *msgq_rsp = p->msgq_rsp;
	struct pipeline_msg_rsp *rsp;
	int status;

	/* send */
	do {
		status = rte_ring_sp_enqueue(msgq_req, req);
	} while (status == -ENOBUFS);

	/* recv */
	do {
		status = rte_ring_sc_dequeue(msgq_rsp, (void **) &rsp);
	} while (status != 0);

	return rsp;
}

int
pipeline_port_in_enable(const char *pipeline_name,
	uint32_t port_id)
{
	struct pipeline *p;
	struct pipeline_msg_req *req;
	struct pipeline_msg_rsp *rsp;
	int status;

	/* Check input params */
	if (pipeline_name == NULL)
		return -1;

	p = pipeline_find(pipeline_name);
	if ((p == NULL) ||
		(p->enabled == 0) ||
		(port_id >= p->n_ports_in))
		return -1;

	/* Allocate request */
	req = pipeline_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = PIPELINE_REQ_PORT_IN_ENABLE;
	req->id = port_id;

	/* Send request and wait for response */
	rsp = pipeline_msg_send_recv(p, req);
	if (rsp == NULL)
		return -1;

	/* Read response */
	status = rsp->status;

	/* Free response */
	pipeline_msg_free(rsp);

	return status;
}

int
pipeline_port_in_disable(const char *pipeline_name,
	uint32_t port_id)
{
	struct pipeline *p;
	struct pipeline_msg_req *req;
	struct pipeline_msg_rsp *rsp;
	int status;

	/* Check input params */
	if (pipeline_name == NULL)
		return -1;

	p = pipeline_find(pipeline_name);
	if ((p == NULL) ||
		(p->enabled == 0) ||
		(port_id >= p->n_ports_in))
		return -1;

	/* Allocate request */
	req = pipeline_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = PIPELINE_REQ_PORT_IN_DISABLE;
	req->id = port_id;

	/* Send request and wait for response */
	rsp = pipeline_msg_send_recv(p, req);
	if (rsp == NULL)
		return -1;

	/* Read response */
	status = rsp->status;

	/* Free response */
	pipeline_msg_free(rsp);

	return status;
}


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

static struct pipeline_msg_rsp *
pipeline_msg_handle_port_in_enable(struct pipeline_data *p,
	struct pipeline_msg_req *req)
{
	struct pipeline_msg_rsp *rsp = (struct pipeline_msg_rsp *) req;
	uint32_t port_id = req->id;

	rsp->status = rte_pipeline_port_in_enable(p->p,
		port_id);

	return rsp;
}

static struct pipeline_msg_rsp *
pipeline_msg_handle_port_in_disable(struct pipeline_data *p,
	struct pipeline_msg_req *req)
{
	struct pipeline_msg_rsp *rsp = (struct pipeline_msg_rsp *) req;
	uint32_t port_id = req->id;

	rsp->status = rte_pipeline_port_in_disable(p->p,
		port_id);

	return rsp;
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
		case PIPELINE_REQ_PORT_IN_ENABLE:
			rsp = pipeline_msg_handle_port_in_enable(p, req);
			break;

		case PIPELINE_REQ_PORT_IN_DISABLE:
			rsp = pipeline_msg_handle_port_in_disable(p, req);
			break;

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
