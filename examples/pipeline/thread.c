/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdlib.h>
#include <errno.h>

#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_lcore.h>

#include "obj.h"
#include "thread.h"

#ifndef THREAD_PIPELINES_MAX
#define THREAD_PIPELINES_MAX                               256
#endif

#ifndef THREAD_BLOCKS_MAX
#define THREAD_BLOCKS_MAX                                  256
#endif

/* Pipeline instruction quanta: Needs to be big enough to do some meaningful
 * work, but not too big to avoid starving any other pipelines mapped to the
 * same thread. For a pipeline that executes 10 instructions per packet, a
 * quanta of 1000 instructions equates to processing 100 packets.
 */
#ifndef PIPELINE_INSTR_QUANTA
#define PIPELINE_INSTR_QUANTA                              1000
#endif

/**
 * In this design, there is a single control plane (CP) thread and one or multiple data plane (DP)
 * threads. Each DP thread can run up to THREAD_PIPELINES_MAX pipelines and up to THREAD_BLOCKS_MAX
 * blocks.
 *
 * The pipelines and blocks are single threaded, meaning that a given pipeline/block can be run by a
 * single thread at any given time, so the same pipeline/block cannot show up in the list of
 * pipelines/blocks of more than one thread at any specific moment.
 *
 * Each DP thread has its own context (struct thread instance), which it shares with the CP thread:
 *  - Read-write by the CP thread;
 *  - Read-only by the DP thread.
 */
struct block {
	block_run_f block_func;
	void *block;
};

struct thread {
	struct rte_swx_pipeline *pipelines[THREAD_PIPELINES_MAX];
	struct block *blocks[THREAD_BLOCKS_MAX];
	volatile uint64_t n_pipelines;
	volatile uint64_t n_blocks;
	int enabled;
} __rte_cache_aligned;

static struct thread threads[RTE_MAX_LCORE];

/**
 * Control plane (CP) thread.
 */
int
thread_init(void)
{
	uint32_t thread_id;
	int status = 0;

	RTE_LCORE_FOREACH_WORKER(thread_id) {
		struct thread *t = &threads[thread_id];
		uint32_t i;

		t->enabled = 1;

		for (i = 0; i < THREAD_BLOCKS_MAX; i++) {
			struct block *b;

			b = calloc(1, sizeof(struct block));
			if (!b) {
				status = -ENOMEM;
				goto error;
			}

			t->blocks[i] = b;
		}
	}

	return 0;

error:
	RTE_LCORE_FOREACH_WORKER(thread_id) {
		struct thread *t = &threads[thread_id];
		uint32_t i;

		t->enabled = 0;

		for (i = 0; i < THREAD_BLOCKS_MAX; i++) {
			free(t->blocks[i]);
			t->blocks[i] = NULL;
		}
	}

	return status;
}

static uint32_t
pipeline_find(struct rte_swx_pipeline *p)
{
	uint32_t thread_id;

	for (thread_id = 0; thread_id < RTE_MAX_LCORE; thread_id++) {
		struct thread *t = &threads[thread_id];
		uint32_t i;

		if (!t->enabled)
			continue;

		for (i = 0; i < t->n_pipelines; i++)
			if (t->pipelines[i] == p)
				break;
	}

	return thread_id;
}

static uint32_t
block_find(void *b)
{
	uint32_t thread_id;

	for (thread_id = 0; thread_id < RTE_MAX_LCORE; thread_id++) {
		struct thread *t = &threads[thread_id];
		uint32_t i;

		if (!t->enabled)
			continue;

		for (i = 0; i < t->n_blocks; i++)
			if (t->blocks[i]->block == b)
				break;
	}

	return thread_id;
}

/**
 * Enable a given pipeline to run on a specific DP thread.
 *
 * CP thread:
 *  - Adds a new pipeline to the end of the DP thread pipeline list (t->pipelines[]);
 *  - Increments the DP thread number of pipelines (t->n_pipelines). It is important to make sure
 *    that t->pipelines[] update is completed BEFORE the t->n_pipelines update, hence the memory
 *    write barrier used below.
 *
 * DP thread:
 *  - Reads t->n_pipelines before starting every new iteration through t->pipelines[]. It detects
 *    the new pipeline when it sees the updated t->n_pipelines value;
 *  - If somehow the above condition is not met, so t->n_pipelines update is incorrectly taking
 *    place before the t->pipelines[] update is completed, then the DP thread will use an incorrect
 *    handle for the new pipeline, which can result in memory corruption or segmentation fault.
 */
int
pipeline_enable(struct rte_swx_pipeline *p, uint32_t thread_id)
{
	struct thread *t;
	uint64_t n_pipelines;

	/* Check input params */
	if (!p || thread_id >= RTE_MAX_LCORE)
		return -EINVAL;

	if (pipeline_find(p) < RTE_MAX_LCORE)
		return -EEXIST;

	t = &threads[thread_id];
	if (!t->enabled)
		return -EINVAL;

	n_pipelines = t->n_pipelines;

	/* Check there is room for at least one more pipeline. */
	if (n_pipelines >= THREAD_PIPELINES_MAX)
		return -ENOSPC;

	/* Install the new pipeline. */
	t->pipelines[n_pipelines] = p;
	rte_wmb();
	t->n_pipelines = n_pipelines + 1;

	return 0;
}

/**
 * Disable a given pipeline from running on any DP thread.
 *
 * CP thread:
 *  - Detects the thread that is running the given pipeline, if any;
 *  - Writes the last pipeline handle (pipeline_last = t->pipelines[t->n_pipelines - 1]) on the
 *    position of the pipeline to be disabled (t->pipelines[i] = pipeline_last) and decrements the
 *    number of pipelines running on the current thread (t->n_pipelines--). This approach makes sure
 *    that no holes with invalid locations are ever developed within the t->pipelines[] array.
 *  - If the memory barrier below is present, then t->n_pipelines update is guaranteed to take place
 *    after the t->pipelines[] update is completed. The possible DP thread behaviors are detailed
 *    below, which are all valid:
 *     - Not run the removed pipeline at all, run all the other pipelines (including pipeline_last)
 *       exactly one time during the current dispatch loop iteration. This takes place when the DP
 *       thread sees the final value of t->n_pipelines;
 *     - Not run the removed pipeline at all, run all the other pipelines, except pipeline_last,
 *       exactly one time and the pipeline_last exactly two times during the current dispatch loop
 *       iteration. This takes place when the DP thread sees the initial value of t->n_pipelines.
 *  - If the memory barrier below is not present, then the t->n_pipelines update may be reordered by
 *    the CPU, so that it takes place before the t->pipelines[] update. The possible DP thread
 *    behaviors are detailed below, which are all valid:
 *     - Not run the removed pipeline at all, run all the other pipelines (including pipeline_last)
 *       exactly one time during the current dispatch loop iteration. This takes place when the DP
 *       thread sees the final values of the t->pipeline[] array;
 *     - Run the removed pipeline one last time, run all the other pipelines exactly one time, with
 *       the exception of the pipeline_last, which is not run during the current dispatch loop
 *       iteration. This takes place when the DP thread sees the initial values of t->pipeline[].
 *
 * DP thread:
 *  - Reads t->n_pipelines before starting every new iteration through t->pipelines[].
 */
void
pipeline_disable(struct rte_swx_pipeline *p)
{
	struct thread *t;
	uint64_t n_pipelines;
	uint32_t thread_id, i;

	/* Check input params */
	if (!p)
		return;

	/* Find the thread that runs this pipeline. */
	thread_id = pipeline_find(p);
	if (thread_id == RTE_MAX_LCORE)
		return;

	t = &threads[thread_id];
	n_pipelines = t->n_pipelines;

	for (i = 0; i < n_pipelines; i++) {
		struct rte_swx_pipeline *pipeline = t->pipelines[i];

		if (pipeline != p)
			continue;

		if (i < n_pipelines - 1) {
			struct rte_swx_pipeline *pipeline_last = t->pipelines[n_pipelines - 1];

			t->pipelines[i] = pipeline_last;
		}

		rte_wmb();
		t->n_pipelines = n_pipelines - 1;

		return;
	}

	return;
}

int
block_enable(block_run_f block_func, void *block, uint32_t thread_id)
{
	struct thread *t;
	uint64_t n_blocks;

	/* Check input params */
	if (!block_func || !block || thread_id >= RTE_MAX_LCORE)
		return -EINVAL;

	if (block_find(block) < RTE_MAX_LCORE)
		return -EEXIST;

	t = &threads[thread_id];
	if (!t->enabled)
		return -EINVAL;

	n_blocks = t->n_blocks;

	/* Check there is room for at least one more block. */
	if (n_blocks >= THREAD_BLOCKS_MAX)
		return -ENOSPC;

	/* Install the new block. */
	t->blocks[n_blocks]->block_func = block_func;
	t->blocks[n_blocks]->block = block;

	rte_wmb();
	t->n_blocks = n_blocks + 1;

	return 0;
}

void
block_disable(void *block)
{
	struct thread *t;
	uint64_t n_blocks;
	uint32_t thread_id, i;

	/* Check input params */
	if (!block)
		return;

	/* Find the thread that runs this block. */
	thread_id = block_find(block);
	if (thread_id == RTE_MAX_LCORE)
		return;

	t = &threads[thread_id];
	n_blocks = t->n_blocks;

	for (i = 0; i < n_blocks; i++) {
		struct block *b = t->blocks[i];

		if (block != b->block)
			continue;

		if (i < n_blocks - 1) {
			struct block *block_last = t->blocks[n_blocks - 1];

			t->blocks[i] = block_last;
		}

		rte_wmb();
		t->n_blocks = n_blocks - 1;

		rte_wmb();
		t->blocks[n_blocks - 1] = b;

		return;
	}
}

/**
 * Data plane (DP) threads.
 *


 * The t->n_pipelines variable is modified by the CP thread every time changes to the t->pipeline[]
 * array are operated, so it is therefore very important that the latest value of t->n_pipelines is
 * read by the DP thread at the beginning of every new dispatch loop iteration, otherwise a stale
 * t->n_pipelines value may result in new pipelines not being detected, running pipelines that have
 * been removed and are possibly no longer valid (e.g. when the pipeline_last is removed), running
 * one pipeline (pipeline_last) twice as frequently than the rest of the pipelines (e.g. when a
 * pipeline other than pipeline_last is removed), etc. This is the reason why t->n_pipelines is
 * marked as volatile.
 */
int
thread_main(void *arg __rte_unused)
{
	struct thread *t;
	uint32_t thread_id;

	thread_id = rte_lcore_id();
	t = &threads[thread_id];

	/* Dispatch loop. */
	for ( ; ; ) {
		uint32_t i;

		/* Pipelines. */
		for (i = 0; i < t->n_pipelines; i++)
			rte_swx_pipeline_run(t->pipelines[i], PIPELINE_INSTR_QUANTA);

		/* Blocks. */
		for (i = 0; i < t->n_blocks; i++) {
			struct block *b = t->blocks[i];

			b->block_func(b->block);
		}
	}

	return 0;
}
