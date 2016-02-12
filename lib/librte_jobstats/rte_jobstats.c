/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <rte_errno.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_branch_prediction.h>

#include "rte_jobstats.h"

#define ADD_TIME_MIN_MAX(obj, type, value) do {      \
	typeof(value) tmp = (value);                     \
	(obj)->type ## _time += tmp;                     \
	if (tmp < (obj)->min_ ## type ## _time)          \
		(obj)->min_ ## type ## _time = tmp;          \
	if (tmp > (obj)->max_ ## type ## _time)          \
		(obj)->max_ ## type ## _time = tmp;          \
} while (0)

#define RESET_TIME_MIN_MAX(obj, type) do {           \
	(obj)->type ## _time = 0;                        \
	(obj)->min_ ## type ## _time = UINT64_MAX;       \
	(obj)->max_ ## type ## _time = 0;                \
} while (0)

static inline uint64_t
get_time(void)
{
	rte_rmb();
	return rte_get_timer_cycles();
}

/* Those are steps used to adjust job period.
 * Experiments show that for forwarding apps the up step must be less than down
 * step to achieve optimal performance.
 */
#define JOB_UPDATE_STEP_UP    1
#define JOB_UPDATE_STEP_DOWN  4

/*
 * Default update function that implements simple period adjustment.
 */
static void
default_update_function(struct rte_jobstats *job, int64_t result)
{
	int64_t err = job->target - result;

	/* Job is happy. Nothing to do */
	if (err == 0)
		return;

	if (err > 0) {
		if (job->period + JOB_UPDATE_STEP_UP < job->max_period)
			job->period += JOB_UPDATE_STEP_UP;
	} else {
		if (job->min_period + JOB_UPDATE_STEP_DOWN < job->period)
			job->period -= JOB_UPDATE_STEP_DOWN;
	}
}

int
rte_jobstats_context_init(struct rte_jobstats_context *ctx)
{
	if (ctx == NULL)
		return -EINVAL;

	/* Init only needed parameters. Zero out everything else. */
	memset(ctx, 0, sizeof(struct rte_jobstats_context));

	rte_jobstats_context_reset(ctx);

	return 0;
}

void
rte_jobstats_context_start(struct rte_jobstats_context *ctx)
{
	uint64_t now;

	ctx->loop_executed_jobs = 0;

	now = get_time();
	ADD_TIME_MIN_MAX(ctx, management, now - ctx->state_time);
	ctx->state_time = now;
}

void
rte_jobstats_context_finish(struct rte_jobstats_context *ctx)
{
	uint64_t now;

	if (likely(ctx->loop_executed_jobs))
		ctx->loop_cnt++;

	now = get_time();
	ADD_TIME_MIN_MAX(ctx, management, now - ctx->state_time);
	ctx->state_time = now;
}

void
rte_jobstats_context_reset(struct rte_jobstats_context *ctx)
{
	RESET_TIME_MIN_MAX(ctx, exec);
	RESET_TIME_MIN_MAX(ctx, management);
	ctx->start_time = get_time();
	ctx->state_time = ctx->start_time;
	ctx->job_exec_cnt = 0;
	ctx->loop_cnt = 0;
}

void
rte_jobstats_set_target(struct rte_jobstats *job, int64_t target)
{
	job->target = target;
}

int
rte_jobstats_start(struct rte_jobstats_context *ctx, struct rte_jobstats *job)
{
	uint64_t now;

	/* Some sanity check. */
	if (unlikely(ctx == NULL || job == NULL || job->context != NULL))
		return -EINVAL;

	/* Link job with context object. */
	job->context = ctx;

	now = get_time();
	ADD_TIME_MIN_MAX(ctx, management, now - ctx->state_time);
	ctx->state_time = now;

	return 0;
}

int
rte_jobstats_abort(struct rte_jobstats *job)
{
	struct rte_jobstats_context *ctx;
	uint64_t now, exec_time;

	/* Some sanity check. */
	if (unlikely(job == NULL || job->context == NULL))
		return -EINVAL;

	ctx = job->context;
	now = get_time();
	exec_time = now - ctx->state_time;
	ADD_TIME_MIN_MAX(ctx, management, exec_time);
	ctx->state_time = now;
	job->context = NULL;

	return 0;
}

int
rte_jobstats_finish(struct rte_jobstats *job, int64_t job_value)
{
	struct rte_jobstats_context *ctx;
	uint64_t now, exec_time;
	int need_update;

	/* Some sanity check. */
	if (unlikely(job == NULL || job->context == NULL))
		return -EINVAL;

	need_update = job->target != job_value;
	/* Adjust period only if job is unhappy of its current period. */
	if (need_update)
		(*job->update_period_cb)(job, job_value);

	ctx = job->context;

	/* Update execution time is considered as runtime so get time after it is
	 * executed. */
	now = get_time();
	exec_time = now - ctx->state_time;
	ADD_TIME_MIN_MAX(job, exec, exec_time);
	ADD_TIME_MIN_MAX(ctx, exec, exec_time);

	ctx->state_time = now;

	ctx->loop_executed_jobs++;
	ctx->job_exec_cnt++;

	job->exec_cnt++;
	job->context = NULL;

	return need_update;
}

void
rte_jobstats_set_period(struct rte_jobstats *job, uint64_t period,
		uint8_t saturate)
{
	if (saturate != 0) {
		if (period < job->min_period)
			period = job->min_period;
		else if (period > job->max_period)
			period = job->max_period;
	}

	job->period = period;
}

void
rte_jobstats_set_min(struct rte_jobstats *job, uint64_t period)
{
	job->min_period = period;
	if (job->period < period)
		job->period = period;
}

void
rte_jobstats_set_max(struct rte_jobstats *job, uint64_t period)
{
	job->max_period = period;
	if (job->period > period)
		job->period = period;
}

int
rte_jobstats_init(struct rte_jobstats *job, const char *name,
		uint64_t min_period, uint64_t max_period, uint64_t initial_period,
		int64_t target)
{
	if (job == NULL)
		return -EINVAL;

	job->period = initial_period;
	job->min_period = min_period;
	job->max_period = max_period;
	job->target = target;
	job->update_period_cb = &default_update_function;
	rte_jobstats_reset(job);
	snprintf(job->name, RTE_DIM(job->name), "%s", name == NULL ? "" : name);
	job->context = NULL;

	return 0;
}

void
rte_jobstats_set_update_period_function(struct rte_jobstats *job,
		rte_job_update_period_cb_t update_period_cb)
{
	if (update_period_cb == NULL)
		update_period_cb = default_update_function;

	job->update_period_cb = update_period_cb;
}

void
rte_jobstats_reset(struct rte_jobstats *job)
{
	RESET_TIME_MIN_MAX(job, exec);
	job->exec_cnt = 0;
}
