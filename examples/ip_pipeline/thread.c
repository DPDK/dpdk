/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_pipeline.h>

#include "pipeline_common_be.h"
#include "app.h"
#include "thread.h"

#if APP_THREAD_HEADROOM_STATS_COLLECT

#define PIPELINE_RUN_REGULAR(thread, pipeline)		\
do {							\
	uint64_t t0 = rte_rdtsc_precise();		\
	int n_pkts = rte_pipeline_run(pipeline->p);	\
							\
	if (n_pkts == 0) {				\
		uint64_t t1 = rte_rdtsc_precise();	\
							\
		thread->headroom_cycles += t1 - t0;	\
	}						\
} while (0)


#define PIPELINE_RUN_CUSTOM(thread, data)		\
do {							\
	uint64_t t0 = rte_rdtsc_precise();		\
	int n_pkts = data->f_run(data->be);		\
							\
	if (n_pkts == 0) {				\
		uint64_t t1 = rte_rdtsc_precise();	\
							\
		thread->headroom_cycles += t1 - t0;	\
	}						\
} while (0)

#else

#define PIPELINE_RUN_REGULAR(thread, pipeline)		\
	rte_pipeline_run(pipeline->p)

#define PIPELINE_RUN_CUSTOM(thread, data)		\
	data->f_run(data->be)

#endif

static inline void *
thread_msg_recv(struct rte_ring *r)
{
	void *msg;
	int status = rte_ring_sc_dequeue(r, &msg);

	if (status != 0)
		return NULL;

	return msg;
}

static inline void
thread_msg_send(struct rte_ring *r,
	void *msg)
{
	int status;

	do {
		status = rte_ring_sp_enqueue(r, msg);
	} while (status == -ENOBUFS);
}

static int
thread_pipeline_enable(struct app_thread_data *t,
		struct thread_pipeline_enable_msg_req *req)
{
	struct app_thread_pipeline_data *p;

	if (req->f_run == NULL) {
		if (t->n_regular >= APP_MAX_THREAD_PIPELINES)
			return -1;
	} else {
		if (t->n_custom >= APP_MAX_THREAD_PIPELINES)
			return -1;
	}

	p = (req->f_run == NULL) ?
		&t->regular[t->n_regular] :
		&t->custom[t->n_custom];

	p->pipeline_id = req->pipeline_id;
	p->be = req->be;
	p->f_run = req->f_run;
	p->f_timer = req->f_timer;
	p->timer_period = req->timer_period;
	p->deadline = 0;

	if (req->f_run == NULL)
		t->n_regular++;
	else
		t->n_custom++;

	return 0;
}

static int
thread_pipeline_disable(struct app_thread_data *t,
		struct thread_pipeline_disable_msg_req *req)
{
	uint32_t n_regular = RTE_MIN(t->n_regular, RTE_DIM(t->regular));
	uint32_t n_custom = RTE_MIN(t->n_custom, RTE_DIM(t->custom));
	uint32_t i;

	/* search regular pipelines of current thread */
	for (i = 0; i < n_regular; i++) {
		if (t->regular[i].pipeline_id != req->pipeline_id)
			continue;

		if (i < n_regular - 1)
			memcpy(&t->regular[i],
			  &t->regular[i+1],
			  (n_regular - 1 - i) * sizeof(struct app_thread_pipeline_data));

		n_regular--;
		t->n_regular = n_regular;

		return 0;
	}

	/* search custom pipelines of current thread */
	for (i = 0; i < n_custom; i++) {
		if (t->custom[i].pipeline_id != req->pipeline_id)
			continue;

		if (i < n_custom - 1)
			memcpy(&t->custom[i],
			  &t->custom[i+1],
			  (n_custom - 1 - i) * sizeof(struct app_thread_pipeline_data));

		n_custom--;
		t->n_custom = n_custom;

		return 0;
	}

	/* return if pipeline not found */
	return -1;
}

static int
thread_msg_req_handle(struct app_thread_data *t)
{
	void *msg_ptr;
	struct thread_msg_req *req;
	struct thread_msg_rsp *rsp;

	msg_ptr = thread_msg_recv(t->msgq_in);
	req = msg_ptr;
	rsp = msg_ptr;

	if (req != NULL)
		switch (req->type) {
		case THREAD_MSG_REQ_PIPELINE_ENABLE: {
			rsp->status = thread_pipeline_enable(t,
					(struct thread_pipeline_enable_msg_req *) req);
			thread_msg_send(t->msgq_out, rsp);
			break;
		}

		case THREAD_MSG_REQ_PIPELINE_DISABLE: {
			rsp->status = thread_pipeline_disable(t,
					(struct thread_pipeline_disable_msg_req *) req);
			thread_msg_send(t->msgq_out, rsp);
			break;
		}

		case THREAD_MSG_REQ_HEADROOM_READ: {
			struct thread_headroom_read_msg_rsp *rsp =
				(struct thread_headroom_read_msg_rsp *)
				req;

			rsp->headroom_ratio = t->headroom_ratio;
			rsp->status = 0;
			thread_msg_send(t->msgq_out, rsp);
			break;
		}
		default:
			break;
		}

	return 0;
}

static void
thread_headroom_update(struct app_thread_data *t, uint64_t time)
{
	uint64_t time_diff = time - t->headroom_time;

	t->headroom_ratio =
		((double) t->headroom_cycles) / ((double) time_diff);

	t->headroom_cycles = 0;
	t->headroom_time = rte_rdtsc_precise();
}

int
app_thread(void *arg)
{
	struct app_params *app = (struct app_params *) arg;
	uint32_t core_id = rte_lcore_id(), i, j;
	struct app_thread_data *t = &app->thread_data[core_id];

	for (i = 0; ; i++) {
		uint32_t n_regular = RTE_MIN(t->n_regular, RTE_DIM(t->regular));
		uint32_t n_custom = RTE_MIN(t->n_custom, RTE_DIM(t->custom));

		/* Run regular pipelines */
		for (j = 0; j < n_regular; j++) {
			struct app_thread_pipeline_data *data = &t->regular[j];
			struct pipeline *p = data->be;

			PIPELINE_RUN_REGULAR(t, p);
		}

		/* Run custom pipelines */
		for (j = 0; j < n_custom; j++) {
			struct app_thread_pipeline_data *data = &t->custom[j];

			PIPELINE_RUN_CUSTOM(t, data);
		}

		/* Timer */
		if ((i & 0xF) == 0) {
			uint64_t time = rte_get_tsc_cycles();
			uint64_t t_deadline = UINT64_MAX;

			if (time < t->deadline)
				continue;

			/* Timer for regular pipelines */
			for (j = 0; j < n_regular; j++) {
				struct app_thread_pipeline_data *data =
					&t->regular[j];
				uint64_t p_deadline = data->deadline;

				if (p_deadline <= time) {
					data->f_timer(data->be);
					p_deadline = time + data->timer_period;
					data->deadline = p_deadline;
				}

				if (p_deadline < t_deadline)
					t_deadline = p_deadline;
			}

			/* Timer for custom pipelines */
			for (j = 0; j < n_custom; j++) {
				struct app_thread_pipeline_data *data =
					&t->custom[j];
				uint64_t p_deadline = data->deadline;

				if (p_deadline <= time) {
					data->f_timer(data->be);
					p_deadline = time + data->timer_period;
					data->deadline = p_deadline;
				}

				if (p_deadline < t_deadline)
					t_deadline = p_deadline;
			}

			/* Timer for thread message request */
			{
				uint64_t deadline = t->thread_req_deadline;

				if (deadline <= time) {
					thread_msg_req_handle(t);
					thread_headroom_update(t, time);
					deadline = time + t->timer_period;
					t->thread_req_deadline = deadline;
				}

				if (deadline < t_deadline)
					t_deadline = deadline;
			}


			t->deadline = t_deadline;
		}
	}

	return 0;
}
