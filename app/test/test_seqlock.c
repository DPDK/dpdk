/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Ericsson AB
 */

#include <rte_seqlock.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_random.h>

#include <inttypes.h>

#include "test.h"

struct data {
	rte_seqlock_t lock;

	uint64_t a;
	uint64_t b __rte_cache_aligned;
	uint64_t c __rte_cache_aligned;
} __rte_cache_aligned;

struct reader {
	struct data *data;
	uint8_t stop;
};

#define WRITER_RUNTIME 2.0 /* s */

#define WRITER_MAX_DELAY 100 /* us */

#define INTERRUPTED_WRITER_FREQUENCY 1000
#define WRITER_INTERRUPT_TIME 1 /* us */

static int
writer_run(void *arg)
{
	struct data *data = arg;
	uint64_t deadline;

	deadline = rte_get_timer_cycles() +
		WRITER_RUNTIME * rte_get_timer_hz();

	while (rte_get_timer_cycles() < deadline) {
		bool interrupted;
		uint64_t new_value;
		unsigned int delay;

		new_value = rte_rand();

		interrupted = rte_rand_max(INTERRUPTED_WRITER_FREQUENCY) == 0;

		rte_seqlock_write_lock(&data->lock);

		data->c = new_value;
		data->b = new_value;

		if (interrupted)
			rte_delay_us_block(WRITER_INTERRUPT_TIME);

		data->a = new_value;

		rte_seqlock_write_unlock(&data->lock);

		delay = rte_rand_max(WRITER_MAX_DELAY);

		rte_delay_us_block(delay);
	}

	return TEST_SUCCESS;
}

#define INTERRUPTED_READER_FREQUENCY 1000
#define READER_INTERRUPT_TIME 1000 /* us */

static int
reader_run(void *arg)
{
	struct reader *r = arg;
	int rc = TEST_SUCCESS;

	while (__atomic_load_n(&r->stop, __ATOMIC_RELAXED) == 0 &&
			rc == TEST_SUCCESS) {
		struct data *data = r->data;
		bool interrupted;
		uint32_t sn;
		uint64_t a;
		uint64_t b;
		uint64_t c;

		interrupted = rte_rand_max(INTERRUPTED_READER_FREQUENCY) == 0;

		do {
			sn = rte_seqlock_read_begin(&data->lock);

			a = data->a;
			if (interrupted)
				rte_delay_us_block(READER_INTERRUPT_TIME);
			c = data->c;
			b = data->b;

		} while (rte_seqlock_read_retry(&data->lock, sn));

		if (a != b || b != c) {
			printf("Reader observed inconsistent data values "
				"%" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
				a, b, c);
			rc = TEST_FAILED;
		}
	}

	return rc;
}

static void
reader_stop(struct reader *reader)
{
	__atomic_store_n(&reader->stop, 1, __ATOMIC_RELAXED);
}

#define NUM_WRITERS 2 /* main lcore + one worker */
#define MIN_NUM_READERS 2
#define MIN_LCORE_COUNT (NUM_WRITERS + MIN_NUM_READERS)

/* Only a compile-time test */
static rte_seqlock_t __rte_unused static_init_lock = RTE_SEQLOCK_INITIALIZER;

static int
test_seqlock(void)
{
	struct reader readers[RTE_MAX_LCORE];
	unsigned int num_lcores;
	unsigned int num_readers;
	struct data *data;
	unsigned int i;
	unsigned int lcore_id;
	unsigned int reader_lcore_ids[RTE_MAX_LCORE];
	unsigned int worker_writer_lcore_id = 0;
	int rc = TEST_SUCCESS;

	num_lcores = rte_lcore_count();

	if (num_lcores < MIN_LCORE_COUNT) {
		printf("Too few cores to run test. Skipping.\n");
		return TEST_SKIPPED;
	}

	num_readers = num_lcores - NUM_WRITERS;

	data = rte_zmalloc(NULL, sizeof(struct data), 0);

	if (data == NULL) {
		printf("Failed to allocate memory for seqlock data\n");
		return TEST_FAILED;
	}

	i = 0;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (i == 0) {
			rte_eal_remote_launch(writer_run, data, lcore_id);
			worker_writer_lcore_id = lcore_id;
		} else {
			unsigned int reader_idx = i - 1;
			struct reader *reader = &readers[reader_idx];

			reader->data = data;
			reader->stop = 0;

			rte_eal_remote_launch(reader_run, reader, lcore_id);
			reader_lcore_ids[reader_idx] = lcore_id;
		}
		i++;
	}

	if (writer_run(data) != 0 ||
			rte_eal_wait_lcore(worker_writer_lcore_id) != 0)
		rc = TEST_FAILED;

	for (i = 0; i < num_readers; i++) {
		reader_stop(&readers[i]);
		if (rte_eal_wait_lcore(reader_lcore_ids[i]) != 0)
			rc = TEST_FAILED;
	}

	rte_free(data);

	return rc;
}

REGISTER_FAST_TEST(seqlock_autotest, true, true, test_seqlock);
