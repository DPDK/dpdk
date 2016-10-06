/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in
 *	 the documentation and/or other materials provided with the
 *	 distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *	 contributors may be used to endorse or promote products derived
 *	 from this software without specific prior written permission.
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
#include <inttypes.h>
#include <locale.h>

#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_spinlock.h>

#include "test.h"

/*
 * Check condition and return an error if true. Assumes that "handle" is the
 * name of the hash structure pointer to be freed.
 */
#define RETURN_IF_ERROR(cond, str, ...) do {                            \
	if (cond) {                                                     \
		printf("ERROR line %d: " str "\n", __LINE__,            \
							##__VA_ARGS__);	\
		if (handle)                                             \
			rte_hash_free(handle);                          \
		return -1;                                              \
	}                                                               \
} while (0)

#define RTE_APP_TEST_HASH_MULTIWRITER_FAILED 0

struct {
	uint32_t *keys;
	uint32_t *found;
	uint32_t nb_tsx_insertion;
	struct rte_hash *h;
} tbl_multiwriter_test_params;

const uint32_t nb_entries = 16*1024*1024;
const uint32_t nb_total_tsx_insertion = 15*1024*1024;
uint32_t rounded_nb_total_tsx_insertion;

static rte_atomic64_t gcycles;
static rte_atomic64_t ginsertions;

static int use_htm;

static int
test_hash_multiwriter_worker(__attribute__((unused)) void *arg)
{
	uint64_t i, offset;
	uint32_t lcore_id = rte_lcore_id();
	uint64_t begin, cycles;

	offset = (lcore_id - rte_get_master_lcore())
		* tbl_multiwriter_test_params.nb_tsx_insertion;

	printf("Core #%d inserting %d: %'"PRId64" - %'"PRId64"\n",
	       lcore_id, tbl_multiwriter_test_params.nb_tsx_insertion,
	       offset, offset + tbl_multiwriter_test_params.nb_tsx_insertion);

	begin = rte_rdtsc_precise();

	for (i = offset;
	     i < offset + tbl_multiwriter_test_params.nb_tsx_insertion;
	     i++) {
		if (rte_hash_add_key(tbl_multiwriter_test_params.h,
				     tbl_multiwriter_test_params.keys + i) < 0)
			break;
	}

	cycles = rte_rdtsc_precise() - begin;
	rte_atomic64_add(&gcycles, cycles);
	rte_atomic64_add(&ginsertions, i - offset);

	for (; i < offset + tbl_multiwriter_test_params.nb_tsx_insertion; i++)
		tbl_multiwriter_test_params.keys[i]
			= RTE_APP_TEST_HASH_MULTIWRITER_FAILED;

	return 0;
}


static int
test_hash_multiwriter(void)
{
	unsigned int i, rounded_nb_total_tsx_insertion;
	static unsigned calledCount = 1;

	uint32_t *keys;
	uint32_t *found;

	struct rte_hash_parameters hash_params = {
		.entries = nb_entries,
		.key_len = sizeof(uint32_t),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};
	if (use_htm)
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT
				| RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	else
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;

	struct rte_hash *handle;
	char name[RTE_HASH_NAMESIZE];

	const void *next_key;
	void *next_data;
	uint32_t iter = 0;

	uint32_t duplicated_keys = 0;
	uint32_t lost_keys = 0;

	snprintf(name, 32, "test%u", calledCount++);
	hash_params.name = name;

	handle = rte_hash_create(&hash_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	tbl_multiwriter_test_params.h = handle;
	tbl_multiwriter_test_params.nb_tsx_insertion =
		nb_total_tsx_insertion / rte_lcore_count();

	rounded_nb_total_tsx_insertion = (nb_total_tsx_insertion /
		tbl_multiwriter_test_params.nb_tsx_insertion)
		* tbl_multiwriter_test_params.nb_tsx_insertion;

	rte_srand(rte_rdtsc());

	keys = rte_malloc(NULL, sizeof(uint32_t) * nb_entries, 0);

	if (keys == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err1;
	}

	found = rte_zmalloc(NULL, sizeof(uint32_t) * nb_entries, 0);
	if (found == NULL) {
		printf("RTE_ZMALLOC failed\n");
		goto err2;
	}

	for (i = 0; i < nb_entries; i++)
		keys[i] = i;

	tbl_multiwriter_test_params.keys = keys;
	tbl_multiwriter_test_params.found = found;

	rte_atomic64_init(&gcycles);
	rte_atomic64_clear(&gcycles);

	rte_atomic64_init(&ginsertions);
	rte_atomic64_clear(&ginsertions);

	/* Fire all threads. */
	rte_eal_mp_remote_launch(test_hash_multiwriter_worker,
				 NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	while (rte_hash_iterate(handle, &next_key, &next_data, &iter) >= 0) {
		/* Search for the key in the list of keys added .*/
		i = *(const uint32_t *)next_key;
		tbl_multiwriter_test_params.found[i]++;
	}

	for (i = 0; i < rounded_nb_total_tsx_insertion; i++) {
		if (tbl_multiwriter_test_params.keys[i]
		    != RTE_APP_TEST_HASH_MULTIWRITER_FAILED) {
			if (tbl_multiwriter_test_params.found[i] > 1) {
				duplicated_keys++;
				break;
			}
			if (tbl_multiwriter_test_params.found[i] == 0) {
				lost_keys++;
				printf("key %d is lost\n", i);
				break;
			}
		}
	}

	if (duplicated_keys > 0) {
		printf("%d key duplicated\n", duplicated_keys);
		goto err3;
	}

	if (lost_keys > 0) {
		printf("%d key lost\n", lost_keys);
		goto err3;
	}

	printf("No key corrupted during multiwriter insertion.\n");

	unsigned long long int cycles_per_insertion =
		rte_atomic64_read(&gcycles)/
		rte_atomic64_read(&ginsertions);

	printf(" cycles per insertion: %llu\n", cycles_per_insertion);

	rte_free(tbl_multiwriter_test_params.found);
	rte_free(tbl_multiwriter_test_params.keys);
	rte_hash_free(handle);
	return 0;

err3:
	rte_free(tbl_multiwriter_test_params.found);
err2:
	rte_free(tbl_multiwriter_test_params.keys);
err1:
	rte_hash_free(handle);
	return -1;
}

static int
test_hash_multiwriter_main(void)
{
	if (rte_lcore_count() == 1) {
		printf("More than one lcore is required to do multiwriter test\n");
		return 0;
	}


	setlocale(LC_NUMERIC, "");


	if (!rte_tm_supported()) {
		printf("Hardware transactional memory (lock elision) "
			"is NOT supported\n");
	} else {
		printf("Hardware transactional memory (lock elision) "
			"is supported\n");

		printf("Test multi-writer with Hardware transactional memory\n");

		use_htm = 1;
		if (test_hash_multiwriter() < 0)
			return -1;
	}

	printf("Test multi-writer without Hardware transactional memory\n");
	use_htm = 0;
	if (test_hash_multiwriter() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(hash_multiwriter_autotest, test_hash_multiwriter_main);
