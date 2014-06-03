/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <rte_malloc.h>

#include <rte_memcpy.h>

#include "test.h"

/*
 * Set this to the maximum buffer size you want to test. If it is 0, then the
 * values in the buf_sizes[] array below will be used.
 */
#define TEST_VALUE_RANGE        0

/* List of buffer sizes to test */
#if TEST_VALUE_RANGE == 0
static size_t buf_sizes[] = {
	0, 1, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255,
	256, 257, 320, 384, 511, 512, 513, 1023, 1024, 1025, 1518, 1522, 1600,
	2048, 3072, 4096, 5120, 6144, 7168, 8192
};
/* MUST be as large as largest packet size above */
#define SMALL_BUFFER_SIZE       8192
#else /* TEST_VALUE_RANGE != 0 */
static size_t buf_sizes[TEST_VALUE_RANGE];
#define SMALL_BUFFER_SIZE       TEST_VALUE_RANGE
#endif /* TEST_VALUE_RANGE == 0 */


/*
 * Arrays of this size are used for measuring uncached memory accesses by
 * picking a random location within the buffer. Make this smaller if there are
 * memory allocation errors.
 */
#define LARGE_BUFFER_SIZE       (100 * 1024 * 1024)

/* How many times to run timing loop for performance tests */
#define TEST_ITERATIONS         1000000
#define TEST_BATCH_SIZE         100

/* Data is aligned on this many bytes (power of 2) */
#define ALIGNMENT_UNIT          16

/*
 * Pointers used in performance tests. The two large buffers are for uncached
 * access where random addresses within the buffer are used for each
 * memcpy. The two small buffers are for cached access.
 */
static uint8_t *large_buf_read, *large_buf_write;
static uint8_t *small_buf_read, *small_buf_write;

/* Initialise data buffers. */
static int
init_buffers(void)
{
	unsigned i;

	large_buf_read = rte_malloc("memcpy", LARGE_BUFFER_SIZE, ALIGNMENT_UNIT);
	if (large_buf_read == NULL)
		goto error_large_buf_read;

	large_buf_write = rte_malloc("memcpy", LARGE_BUFFER_SIZE, ALIGNMENT_UNIT);
	if (large_buf_write == NULL)
		goto error_large_buf_write;

	small_buf_read = rte_malloc("memcpy", SMALL_BUFFER_SIZE, ALIGNMENT_UNIT);
	if (small_buf_read == NULL)
		goto error_small_buf_read;

	small_buf_write = rte_malloc("memcpy", SMALL_BUFFER_SIZE, ALIGNMENT_UNIT);
	if (small_buf_write == NULL)
		goto error_small_buf_write;

	for (i = 0; i < LARGE_BUFFER_SIZE; i++)
		large_buf_read[i] = rte_rand();
	for (i = 0; i < SMALL_BUFFER_SIZE; i++)
		small_buf_read[i] = rte_rand();

	return 0;

error_small_buf_write:
	rte_free(small_buf_read);
error_small_buf_read:
	rte_free(large_buf_write);
error_large_buf_write:
	rte_free(large_buf_read);
error_large_buf_read:
	printf("ERROR: not enough memory\n");
	return -1;
}

/* Cleanup data buffers */
static void
free_buffers(void)
{
	rte_free(large_buf_read);
	rte_free(large_buf_write);
	rte_free(small_buf_read);
	rte_free(small_buf_write);
}

/*
 * Get a random offset into large array, with enough space needed to perform
 * max copy size. Offset is aligned.
 */
static inline size_t
get_rand_offset(void)
{
	return ((rte_rand() % (LARGE_BUFFER_SIZE - SMALL_BUFFER_SIZE)) &
	                ~(ALIGNMENT_UNIT - 1));
}

/* Fill in source and destination addresses. */
static inline void
fill_addr_arrays(size_t *dst_addr, int is_dst_cached,
		size_t *src_addr, int is_src_cached)
{
	unsigned int i;

	for (i = 0; i < TEST_BATCH_SIZE; i++) {
		dst_addr[i] = (is_dst_cached) ? 0 : get_rand_offset();
		src_addr[i] = (is_src_cached) ? 0 : get_rand_offset();
	}
}

/*
 * WORKAROUND: For some reason the first test doing an uncached write
 * takes a very long time (~25 times longer than is expected). So we do
 * it once without timing.
 */
static void
do_uncached_write(uint8_t *dst, int is_dst_cached,
		const uint8_t *src, int is_src_cached, size_t size)
{
	unsigned i, j;
	size_t dst_addrs[TEST_BATCH_SIZE], src_addrs[TEST_BATCH_SIZE];

	for (i = 0; i < (TEST_ITERATIONS / TEST_BATCH_SIZE); i++) {
		fill_addr_arrays(dst_addrs, is_dst_cached,
			 src_addrs, is_src_cached);
		for (j = 0; j < TEST_BATCH_SIZE; j++)
			rte_memcpy(dst+dst_addrs[j], src+src_addrs[j], size);
	}
}

/*
 * Run a single memcpy performance test. This is a macro to ensure that if
 * the "size" parameter is a constant it won't be converted to a variable.
 */
#define SINGLE_PERF_TEST(dst, is_dst_cached, src, is_src_cached, size) do {   \
	unsigned int iter, t;                                                 \
	size_t dst_addrs[TEST_BATCH_SIZE], src_addrs[TEST_BATCH_SIZE];        \
	uint64_t start_time, total_time = 0;                                  \
	uint64_t total_time2 = 0;                                             \
	for (iter = 0; iter < (TEST_ITERATIONS / TEST_BATCH_SIZE); iter++) {  \
		fill_addr_arrays(dst_addrs, is_dst_cached,                    \
		                 src_addrs, is_src_cached);                   \
		start_time = rte_rdtsc();                                     \
		for (t = 0; t < TEST_BATCH_SIZE; t++)                         \
			rte_memcpy(dst+dst_addrs[t], src+src_addrs[t], size); \
		total_time += rte_rdtsc() - start_time;                       \
	}                                                                     \
	for (iter = 0; iter < (TEST_ITERATIONS / TEST_BATCH_SIZE); iter++) {  \
		fill_addr_arrays(dst_addrs, is_dst_cached,                    \
		                 src_addrs, is_src_cached);                   \
		start_time = rte_rdtsc();                                     \
		for (t = 0; t < TEST_BATCH_SIZE; t++)                         \
			memcpy(dst+dst_addrs[t], src+src_addrs[t], size);     \
		total_time2 += rte_rdtsc() - start_time;                      \
	}                                                                     \
	printf("%8.0f -",  (double)total_time /TEST_ITERATIONS);              \
	printf("%5.0f",  (double)total_time2 / TEST_ITERATIONS);              \
} while (0)

/* Run memcpy() tests for each cached/uncached permutation. */
#define ALL_PERF_TESTS_FOR_SIZE(n) do {                             \
	if (__builtin_constant_p(n))                                \
		printf("\nC%6u", (unsigned)n);                      \
	else                                                        \
		printf("\n%7u", (unsigned)n);                       \
	SINGLE_PERF_TEST(small_buf_write, 1, small_buf_read, 1, n); \
	SINGLE_PERF_TEST(large_buf_write, 0, small_buf_read, 1, n); \
	SINGLE_PERF_TEST(small_buf_write, 1, large_buf_read, 0, n); \
	SINGLE_PERF_TEST(large_buf_write, 0, large_buf_read, 0, n); \
} while (0)

/*
 * Run performance tests for a number of different sizes and cached/uncached
 * permutations.
 */
static int
perf_test(void)
{
	const unsigned num_buf_sizes = sizeof(buf_sizes) / sizeof(buf_sizes[0]);
	unsigned i;
	int ret;

	ret = init_buffers();
	if (ret != 0)
		return ret;

#if TEST_VALUE_RANGE != 0
	/* Setup buf_sizes array, if required */
	for (i = 0; i < TEST_VALUE_RANGE; i++)
		buf_sizes[i] = i;
#endif

	/* See function comment */
	do_uncached_write(large_buf_write, 0, small_buf_read, 1, SMALL_BUFFER_SIZE);

	printf("\n** rte_memcpy() - memcpy perf. tests (C = compile-time constant) **\n"
	       "======= ============== ============== ============== ==============\n"
	       "   Size Cache to cache   Cache to mem   Mem to cache     Mem to mem\n"
	       "(bytes)        (ticks)        (ticks)        (ticks)        (ticks)\n"
	       "------- -------------- -------------- -------------- --------------");

	/* Do tests where size is a variable */
	for (i = 0; i < num_buf_sizes; i++) {
		ALL_PERF_TESTS_FOR_SIZE((size_t)buf_sizes[i]);
	}
	printf("\n------- -------------- -------------- -------------- --------------");
	/* Do tests where size is a compile-time constant */
	ALL_PERF_TESTS_FOR_SIZE(63U);
	ALL_PERF_TESTS_FOR_SIZE(64U);
	ALL_PERF_TESTS_FOR_SIZE(65U);
	ALL_PERF_TESTS_FOR_SIZE(255U);
	ALL_PERF_TESTS_FOR_SIZE(256U);
	ALL_PERF_TESTS_FOR_SIZE(257U);
	ALL_PERF_TESTS_FOR_SIZE(1023U);
	ALL_PERF_TESTS_FOR_SIZE(1024U);
	ALL_PERF_TESTS_FOR_SIZE(1025U);
	ALL_PERF_TESTS_FOR_SIZE(1518U);

	printf("\n======= ============== ============== ============== ==============\n\n");

	free_buffers();

	return 0;
}


int
test_memcpy_perf(void)
{
	int ret;

	ret = perf_test();
	if (ret != 0)
		return -1;
	return 0;
}
