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

#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_spinlock.h>
#include <rte_launch.h>

#include "test.h"

/*
 * Check condition and return an error if true. Assumes that "handle" is the
 * name of the hash structure pointer to be freed.
 */
#define RETURN_IF_ERROR(cond, str, ...) do {				\
	if (cond) {							\
		printf("ERROR line %d: " str "\n", __LINE__,		\
							##__VA_ARGS__);	\
		if (handle)						\
			rte_hash_free(handle);				\
		return -1;						\
	}								\
} while (0)

enum locking_mode_t {
	NORMAL_LOCK,
	LOCK_ELISION,
	NULL_LOCK
};

struct {
	uint32_t num_iterations;
	struct rte_hash *h;
	rte_spinlock_t *lock;
	int locking_mode;
} tbl_scaling_test_params;

static rte_atomic64_t gcycles;

static int test_hash_scaling_worker(__attribute__((unused)) void *arg)
{
	uint64_t i, key;
	uint32_t thr_id = rte_sys_gettid();
	uint64_t begin, cycles = 0;

	switch (tbl_scaling_test_params.locking_mode) {

	case NORMAL_LOCK:

		for (i = 0; i < tbl_scaling_test_params.num_iterations; i++) {
			/*	different threads get different keys because
				we use the thread-id in the key computation
			 */
			key = rte_hash_crc(&i, sizeof(i), thr_id);
			begin = rte_rdtsc_precise();
			rte_spinlock_lock(tbl_scaling_test_params.lock);
			rte_hash_add_key(tbl_scaling_test_params.h, &key);
			rte_spinlock_unlock(tbl_scaling_test_params.lock);
			cycles += rte_rdtsc_precise() - begin;
		}
		break;

	case LOCK_ELISION:

		for (i = 0; i < tbl_scaling_test_params.num_iterations; i++) {
			key = rte_hash_crc(&i, sizeof(i), thr_id);
			begin = rte_rdtsc_precise();
			rte_spinlock_lock_tm(tbl_scaling_test_params.lock);
			rte_hash_add_key(tbl_scaling_test_params.h, &key);
			rte_spinlock_unlock_tm(tbl_scaling_test_params.lock);
			cycles += rte_rdtsc_precise() - begin;
		}
		break;

	default:

		for (i = 0; i < tbl_scaling_test_params.num_iterations; i++) {
			key = rte_hash_crc(&i, sizeof(i), thr_id);
			begin = rte_rdtsc_precise();
			rte_hash_add_key(tbl_scaling_test_params.h, &key);
			cycles += rte_rdtsc_precise() - begin;
		}
	}

	rte_atomic64_add(&gcycles, cycles);

	return 0;
}

/*
 * Do scalability perf tests.
 */
static int
test_hash_scaling(int locking_mode)
{
	static unsigned calledCount =    1;
	uint32_t num_iterations = 1024*1024;
	uint64_t i, key;
	struct rte_hash_parameters hash_params = {
		.entries = num_iterations*2,
		.key_len = sizeof(key),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};
	struct rte_hash *handle;
	char name[RTE_HASH_NAMESIZE];
	rte_spinlock_t lock;

	rte_spinlock_init(&lock);

	snprintf(name, 32, "test%u", calledCount++);
	hash_params.name = name;

	handle = rte_hash_create(&hash_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	tbl_scaling_test_params.num_iterations =
		num_iterations/rte_lcore_count();
	tbl_scaling_test_params.h = handle;
	tbl_scaling_test_params.lock = &lock;
	tbl_scaling_test_params.locking_mode = locking_mode;

	rte_atomic64_init(&gcycles);
	rte_atomic64_clear(&gcycles);

	/* fill up to initial size */
	for (i = 0; i < num_iterations; i++) {
		key = rte_hash_crc(&i, sizeof(i), 0xabcdabcd);
		rte_hash_add_key(tbl_scaling_test_params.h, &key);
	}

	rte_eal_mp_remote_launch(test_hash_scaling_worker, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	unsigned long long int cycles_per_operation =
		rte_atomic64_read(&gcycles)/
		(tbl_scaling_test_params.num_iterations*rte_lcore_count());
	const char *lock_name;

	switch (locking_mode) {
	case NORMAL_LOCK:
		lock_name = "normal spinlock";
		break;
	case LOCK_ELISION:
		lock_name = "lock elision";
		break;
	default:
		lock_name = "null lock";
	}
	printf("--------------------------------------------------------\n");
	printf("Cores: %d; %s mode ->  cycles per operation: %llu\n",
		rte_lcore_count(), lock_name, cycles_per_operation);
	printf("--------------------------------------------------------\n");
	/* CSV output */
	printf(">>>%d,%s,%llu\n", rte_lcore_count(), lock_name,
		cycles_per_operation);

	rte_hash_free(handle);
	return 0;
}

static int
test_hash_scaling_main(void)
{
	int r = 0;

	if (rte_lcore_count() == 1)
		r = test_hash_scaling(NULL_LOCK);

	if (r == 0)
		r = test_hash_scaling(NORMAL_LOCK);

	if (!rte_tm_supported()) {
		printf("Hardware transactional memory (lock elision) is NOT supported\n");
		return r;
	}
	printf("Hardware transactional memory (lock elision) is supported\n");

	if (r == 0)
		r = test_hash_scaling(LOCK_ELISION);

	return r;
}


static struct test_command hash_scaling_cmd = {
	.command = "hash_scaling_autotest",
	.callback = test_hash_scaling_main,
};
REGISTER_TEST_COMMAND(hash_scaling_cmd);
