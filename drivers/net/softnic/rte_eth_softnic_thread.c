/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_service_component.h>
#include <rte_ring.h>

#include "rte_eth_softnic_internals.h"

/**
 * Main thread: data plane thread init
 */
void
softnic_thread_free(struct pmd_internals *softnic)
{
	uint32_t i;

	RTE_LCORE_FOREACH_WORKER(i) {
		struct softnic_thread *t = &softnic->thread[i];

		/* MSGQs */
		rte_ring_free(t->msgq_req);

		rte_ring_free(t->msgq_rsp);
	}
}

int
softnic_thread_init(struct pmd_internals *softnic)
{
	uint32_t i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		char ring_name[NAME_MAX];
		struct rte_ring *msgq_req, *msgq_rsp;
		struct softnic_thread *t = &softnic->thread[i];
		struct softnic_thread_data *t_data = &softnic->thread_data[i];
		uint32_t cpu_id = rte_lcore_to_socket_id(i);

		/* MSGQs */
		snprintf(ring_name, sizeof(ring_name), "%s-TH%u-REQ",
			softnic->params.name,
			i);

		msgq_req = rte_ring_create(ring_name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_req == NULL) {
			softnic_thread_free(softnic);
			return -1;
		}

		snprintf(ring_name, sizeof(ring_name), "%s-TH%u-RSP",
			softnic->params.name,
			i);

		msgq_rsp = rte_ring_create(ring_name,
			THREAD_MSGQ_SIZE,
			cpu_id,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (msgq_rsp == NULL) {
			softnic_thread_free(softnic);
			return -1;
		}

		/* Main thread records */
		t->msgq_req = msgq_req;
		t->msgq_rsp = msgq_rsp;
		t->service_id = UINT32_MAX;

		/* Data plane thread records */
		t_data->n_pipelines = 0;
		t_data->msgq_req = msgq_req;
		t_data->msgq_rsp = msgq_rsp;
		t_data->timer_period =
			(rte_get_tsc_hz() * THREAD_TIMER_PERIOD_MS) / 1000;
		t_data->time_next = rte_get_tsc_cycles() + t_data->timer_period;
	}

	return 0;
}

static inline int
thread_is_valid(struct pmd_internals *softnic, uint32_t thread_id)
{
	if (thread_id >= RTE_MAX_LCORE)
		return 0; /* FALSE */

	if (thread_id == rte_get_main_lcore())
		return 0; /* FALSE */

	if (softnic->params.sc && rte_lcore_has_role(thread_id, ROLE_SERVICE))
		return 1; /* TRUE */
	if (!softnic->params.sc && rte_lcore_has_role(thread_id, ROLE_RTE))
		return 1; /* TRUE */

	return 0; /* FALSE */
}

static inline int
thread_is_running(uint32_t thread_id)
{
	enum rte_lcore_state_t thread_state;

	thread_state = rte_eal_get_lcore_state(thread_id);
	return (thread_state == RUNNING)? 1 : 0;
}

static int32_t
rte_pmd_softnic_run_internal(void *arg);

static inline int
thread_sc_service_up(struct pmd_internals *softnic, uint32_t thread_id)
{
	struct rte_service_spec service_params;
	struct softnic_thread *t = &softnic->thread[thread_id];
	struct rte_eth_dev *dev;
	int status;

	/* service params */
	dev = rte_eth_dev_get_by_name(softnic->params.name);
	if (!dev)
		return -EINVAL;

	snprintf(service_params.name, sizeof(service_params.name), "%s_%u",
		softnic->params.name,
		thread_id);
	service_params.callback = rte_pmd_softnic_run_internal;
	service_params.callback_userdata = dev;
	service_params.capabilities = 0;
	service_params.socket_id = (int)softnic->params.cpu_id;

	/* service register */
	status = rte_service_component_register(&service_params, &t->service_id);
	if (status)
		return status;

	status = rte_service_component_runstate_set(t->service_id, 1);
	if (status) {
		rte_service_component_unregister(t->service_id);
		t->service_id = UINT32_MAX;
		return status;
	}

	status = rte_service_runstate_set(t->service_id, 1);
	if (status) {
		rte_service_component_runstate_set(t->service_id, 0);
		rte_service_component_unregister(t->service_id);
		t->service_id = UINT32_MAX;
		return status;
	}

	/* service map to thread */
	status = rte_service_map_lcore_set(t->service_id, thread_id, 1);
	if (status) {
		rte_service_runstate_set(t->service_id, 0);
		rte_service_component_runstate_set(t->service_id, 0);
		rte_service_component_unregister(t->service_id);
		t->service_id = UINT32_MAX;
		return status;
	}

	return 0;
}

static inline void
thread_sc_service_down(struct pmd_internals *softnic, uint32_t thread_id)
{
	struct softnic_thread *t = &softnic->thread[thread_id];

	/* service unmap from thread */
	rte_service_map_lcore_set(t->service_id, thread_id, 0);

	/* service unregister */
	rte_service_runstate_set(t->service_id, 0);
	rte_service_component_runstate_set(t->service_id, 0);
	rte_service_component_unregister(t->service_id);

	t->service_id = UINT32_MAX;
}

void
softnic_thread_pipeline_disable_all(struct pmd_internals *softnic)
{
	uint32_t thread_id;

	for (thread_id = 0; thread_id < RTE_MAX_LCORE; thread_id++) {
		struct softnic_thread_data *td = &softnic->thread_data[thread_id];

		if (!thread_is_valid(softnic, thread_id))
			continue;

		if (softnic->params.sc && td->n_pipelines)
			thread_sc_service_down(softnic, thread_id);

		td->n_pipelines = 0;
	}
}

/**
 * Main thread & data plane threads: message passing
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
			struct rte_swx_pipeline *p;
		} pipeline_enable;

		struct {
			struct rte_swx_pipeline *p;
		} pipeline_disable;
	};
};

struct thread_msg_rsp {
	int status;
};

/**
 * Main thread
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
thread_msg_send_recv(struct pmd_internals *softnic,
	uint32_t thread_id,
	struct thread_msg_req *req)
{
	struct softnic_thread *t = &softnic->thread[thread_id];
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
		status = rte_ring_sc_dequeue(msgq_rsp, (void **)&rsp);
	} while (status != 0);

	return rsp;
}

int
softnic_thread_pipeline_enable(struct pmd_internals *softnic,
	uint32_t thread_id,
	struct pipeline *p)
{
	struct thread_msg_req *req;
	struct thread_msg_rsp *rsp;
	uint32_t n_pipelines;
	int status;

	/* Check input params */
	if (!thread_is_valid(softnic, thread_id) ||
		(p == NULL) ||
		p->enabled)
		return -1;

	n_pipelines = softnic_pipeline_thread_count(softnic, thread_id);
	if (n_pipelines >= THREAD_PIPELINES_MAX)
		return -1;

	if (softnic->params.sc && (n_pipelines == 0)) {
		status = thread_sc_service_up(softnic, thread_id);
		if (status)
			return status;
	}

	if (!thread_is_running(thread_id)) {
		struct softnic_thread_data *td = &softnic->thread_data[thread_id];

		/* Data plane thread */
		td->p[td->n_pipelines] = p->p;
		td->n_pipelines++;

		/* Pipeline */
		p->thread_id = thread_id;
		p->enabled = 1;

		return 0;
	}

	/* Allocate request */
	req = thread_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = THREAD_REQ_PIPELINE_ENABLE;
	req->pipeline_enable.p = p->p;

	/* Send request and wait for response */
	rsp = thread_msg_send_recv(softnic, thread_id, req);

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
softnic_thread_pipeline_disable(struct pmd_internals *softnic,
	uint32_t thread_id,
	struct pipeline *p)
{
	struct thread_msg_req *req;
	struct thread_msg_rsp *rsp;
	uint32_t n_pipelines;
	int status;

	/* Check input params */
	if (!thread_is_valid(softnic, thread_id) ||
		(p == NULL) ||
		(p->enabled && (p->thread_id != thread_id)))
		return -1;

	if (p->enabled == 0)
		return 0;

	if (!thread_is_running(thread_id)) {
		struct softnic_thread_data *td = &softnic->thread_data[thread_id];
		uint32_t i;

		for (i = 0; i < td->n_pipelines; i++) {
			if (td->p[i] != p->p)
				continue;

			/* Data plane thread */
			if (i < td->n_pipelines - 1)
				td->p[i] = td->p[td->n_pipelines - 1];

			td->n_pipelines--;

			/* Pipeline */
			p->enabled = 0;

			break;
		}

		if (softnic->params.sc && (td->n_pipelines == 0))
			thread_sc_service_down(softnic, thread_id);

		return 0;
	}

	/* Allocate request */
	req = thread_msg_alloc();
	if (req == NULL)
		return -1;

	/* Write request */
	req->type = THREAD_REQ_PIPELINE_DISABLE;
	req->pipeline_disable.p = p->p;

	/* Send request and wait for response */
	rsp = thread_msg_send_recv(softnic, thread_id, req);

	/* Read response */
	status = rsp->status;

	/* Free response */
	thread_msg_free(rsp);

	/* Request completion */
	if (status)
		return status;

	p->enabled = 0;

	n_pipelines = softnic_pipeline_thread_count(softnic, thread_id);
	if (softnic->params.sc && (n_pipelines == 0))
		thread_sc_service_down(softnic, thread_id);

	return 0;
}

/**
 * Data plane threads: message handling
 */
static inline struct thread_msg_req *
thread_msg_recv(struct rte_ring *msgq_req)
{
	struct thread_msg_req *req;

	int status = rte_ring_sc_dequeue(msgq_req, (void **)&req);

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
thread_msg_handle_pipeline_enable(struct softnic_thread_data *t,
	struct thread_msg_req *req)
{
	struct thread_msg_rsp *rsp = (struct thread_msg_rsp *)req;

	/* Request */
	t->p[t->n_pipelines] = req->pipeline_enable.p;
	t->n_pipelines++;

	/* Response */
	rsp->status = 0;
	return rsp;
}

static struct thread_msg_rsp *
thread_msg_handle_pipeline_disable(struct softnic_thread_data *t,
	struct thread_msg_req *req)
{
	struct thread_msg_rsp *rsp = (struct thread_msg_rsp *)req;
	uint32_t n_pipelines = t->n_pipelines;
	struct rte_swx_pipeline *pipeline = req->pipeline_disable.p;
	uint32_t i;

	/* find pipeline */
	for (i = 0; i < n_pipelines; i++) {
		if (t->p[i] != pipeline)
			continue;

		if (i < n_pipelines - 1)
			t->p[i] = t->p[n_pipelines - 1];

		t->n_pipelines--;

		rsp->status = 0;
		return rsp;
	}

	/* should not get here */
	rsp->status = 0;
	return rsp;
}

static void
thread_msg_handle(struct softnic_thread_data *t)
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
			rsp = (struct thread_msg_rsp *)req;
			rsp->status = -1;
		}

		thread_msg_send(t->msgq_rsp, rsp);
	}
}

/**
 * Data plane threads: main
 */
static int32_t
rte_pmd_softnic_run_internal(void *arg)
{
	struct rte_eth_dev *dev = arg;
	struct pmd_internals *softnic;
	struct softnic_thread_data *t;
	uint32_t thread_id, j;

	softnic = dev->data->dev_private;
	thread_id = rte_lcore_id();
	t = &softnic->thread_data[thread_id];
	t->iter++;

	/* Data Plane */
	for (j = 0; j < t->n_pipelines; j++)
		rte_swx_pipeline_run(t->p[j], PIPELINE_INSTR_QUANTA);

	/* Control Plane */
	if ((t->iter & 0xFLLU) == 0) {
		uint64_t time = rte_get_tsc_cycles();
		uint64_t time_next = t->time_next;

		if (time < time_next)
			return 0;

		/* Thread message queues */
		thread_msg_handle(t);

		t->time_next = time_next + t->timer_period;
	}

	return 0;
}

int
rte_pmd_softnic_run(uint16_t port_id)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, 0);
#endif

	return (int)rte_pmd_softnic_run_internal(dev);
}
