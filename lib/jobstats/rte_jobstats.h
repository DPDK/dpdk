/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#ifndef JOBSTATS_H_
#define JOBSTATS_H_

#include <stdint.h>

#include <rte_memory.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_JOBSTATS_NAMESIZE 32

/* Forward declarations. */
struct rte_jobstats_context;
struct rte_jobstats;

/**
 * This function should calculate new period and set it using
 * rte_jobstats_set_period() function. Time spent in this function will be
 * added to job's runtime.
 *
 * @param job
 *  The job data structure handler.
 * @param job_result
 *  Result of calling job callback.
 */
typedef void (*rte_job_update_period_cb_t)(struct rte_jobstats *job,
		int64_t job_result);

struct rte_jobstats {
	uint64_t period;
	/**< Estimated period of execution. */

	uint64_t min_period;
	/**< Minimum period. */

	uint64_t max_period;
	/**< Maximum period. */

	int64_t target;
	/**< Desired value for this job. */

	rte_job_update_period_cb_t update_period_cb;
	/**< Period update callback. */

	uint64_t exec_time;
	/**< Total time (sum) that this job was executing. */

	uint64_t min_exec_time;
	/**< Minimum execute time. */

	uint64_t max_exec_time;
	/**< Maximum execute time. */

	uint64_t exec_cnt;
	/**< Execute count. */

	char name[RTE_JOBSTATS_NAMESIZE];
	/**< Name of this job */

	struct rte_jobstats_context *context;
	/**< Job stats context object that is executing this job. */
} __rte_cache_aligned;

struct rte_jobstats_context {
	/** Variable holding time at different points:
	 * -# loop start time if loop was started but no job executed yet.
	 * -# job start time if job is currently executing.
	 * -# job finish time if job finished its execution.
	 * -# loop finish time if loop finished its execution. */
	uint64_t state_time;

	uint64_t loop_executed_jobs;
	/**< Count of executed jobs in this loop. */

	/* Statistics start. */

	uint64_t exec_time;
	/**< Total time taken to execute jobs, not including management time. */

	uint64_t min_exec_time;
	/**< Minimum loop execute time. */

	uint64_t max_exec_time;
	/**< Maximum loop execute time. */

	/**
	 * Sum of time that is not the execute time (ex: from job finish to next
	 * job start).
	 *
	 * This time might be considered as overhead of library + job scheduling.
	 */
	uint64_t management_time;

	uint64_t min_management_time;
	/**< Minimum management time */

	uint64_t max_management_time;
	/**< Maximum management time */

	uint64_t start_time;
	/**< Time since last reset stats. */

	uint64_t job_exec_cnt;
	/**< Total count of executed jobs. */

	uint64_t loop_cnt;
	/**< Total count of executed loops with at least one executed job. */
} __rte_cache_aligned;

/**
 * Initialize given context object with default values.
 *
 * @param ctx
 *  Job stats context object to initialize.
 *
 * @return
 *  0 on success
 *  -EINVAL if *ctx* is NULL
 */
int
rte_jobstats_context_init(struct rte_jobstats_context *ctx);

/**
 * Mark that new set of jobs start executing.
 *
 * @param ctx
 *  Job stats context object.
 */
void
rte_jobstats_context_start(struct rte_jobstats_context *ctx);

/**
 * Mark that there is no more jobs ready to execute in this turn. Calculate
 * stats for this loop turn.
 *
 * @param ctx
 *  Job stats context.
 */
void
rte_jobstats_context_finish(struct rte_jobstats_context *ctx);

/**
 * Function resets job context statistics.
 *
 * @param ctx
 *  Job stats context which statistics will be reset.
 */
void
rte_jobstats_context_reset(struct rte_jobstats_context *ctx);

/**
 * Initialize given job stats object.
 *
 * @param job
 *  Job object.
 * @param name
 *  Optional job name.
 * @param min_period
 *  Minimum period that this job can accept.
 * @param max_period
 *  Maximum period that this job can accept.
 * @param initial_period
 *  Initial period. It will be checked against *min_period* and *max_period*.
 * @param target
 *  Target value that this job try to achieve.
 *
 * @return
 *  0 on success
 *  -EINVAL if *job* is NULL
 */
int
rte_jobstats_init(struct rte_jobstats *job, const char *name,
		uint64_t min_period, uint64_t max_period, uint64_t initial_period,
		int64_t target);

/**
 * Set job desired target value. Difference between target and job value
 * value must be used to properly adjust job execute period value.
 *
 * @param job
 *  The job object.
 * @param target
 *  New target.
 */
void
rte_jobstats_set_target(struct rte_jobstats *job, int64_t target);

/**
 * Mark that *job* is starting of its execution in context of *ctx* object.
 *
 * @param ctx
 *  Job stats context.
 * @param job
 *  Job object.
 * @return
 *  0 on success
 *  -EINVAL if *ctx* or *job* is NULL or *job* is executing in another context
 *  context already,
 */
int
rte_jobstats_start(struct rte_jobstats_context *ctx, struct rte_jobstats *job);

/**
 * Mark that *job* finished its execution, but time of this work will be skipped
 * and added to management time.
 *
 * @param job
 *  Job object.
 *
 * @return
 *  0 on success
 *  -EINVAL if job is NULL or job was not started (it have no context).
 */
int
rte_jobstats_abort(struct rte_jobstats *job);

/**
 * Mark that *job* finished its execution. Context in which it was executing
 * will receive stat update. After this function call *job* object is ready to
 * be executed in other context.
 *
 * @param job
 *  Job object.
 * @param job_value
 *  Job value. Job should pass in this parameter a value that it try to optimize
 *  for example the number of packets it processed.
 *
 * @return
 *  0 if job's period was not updated (job target equals *job_value*)
 *  1 if job's period was updated
 *  -EINVAL if job is NULL or job was not started (it have no context).
 */
int
rte_jobstats_finish(struct rte_jobstats *job, int64_t job_value);

/**
 * Set execute period of given job.
 *
 * @param job
 *  The job object.
 * @param period
 *  New period value.
 * @param saturate
 *  If zero, skip period saturation to min, max range.
 */
void
rte_jobstats_set_period(struct rte_jobstats *job, uint64_t period,
		uint8_t saturate);
/**
 * Set minimum execute period of given job. Current period will be checked
 * against new minimum value.
 *
 * @param job
 *  The job object.
 * @param period
 *  New minimum period value.
 */
void
rte_jobstats_set_min(struct rte_jobstats *job, uint64_t period);
/**
 * Set maximum execute period of given job. Current period will be checked
 * against new maximum value.
 *
 * @param job
 *  The job object.
 * @param period
 *  New maximum period value.
 */
void
rte_jobstats_set_max(struct rte_jobstats *job, uint64_t period);

/**
 * Set update period callback that is invoked after job finish.
 *
 * If application wants to do more sophisticated calculations than default
 * it can provide this handler.
 *
 * @param job
 *  Job object.
 * @param update_period_cb
 *  Callback to set. If NULL restore default update function.
 */
void
rte_jobstats_set_update_period_function(struct rte_jobstats *job,
		rte_job_update_period_cb_t update_period_cb);

/**
 * Function resets job statistics.
 *
 * @param job
 *  Job which statistics will be reset.
 */
void
rte_jobstats_reset(struct rte_jobstats *job);

#ifdef __cplusplus
}
#endif

#endif /* JOBSTATS_H_ */
