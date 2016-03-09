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

#include <string.h>
#include <rte_byteorder.h>
#include <rte_table_lpm_ipv6.h>
#include <rte_lru.h>
#include <rte_cycles.h>
#include "test_table_tables.h"
#include "test_table.h"

table_test table_tests[] = {
	test_table_stub,
	test_table_array,
	test_table_lpm,
	test_table_lpm_ipv6,
	test_table_hash_lru,
	test_table_hash_ext,
};

#define PREPARE_PACKET(mbuf, value) do {				\
	uint32_t *k32, *signature;					\
	uint8_t *key;							\
	mbuf = rte_pktmbuf_alloc(pool);					\
	signature = RTE_MBUF_METADATA_UINT32_PTR(mbuf,			\
			APP_METADATA_OFFSET(0));			\
	key = RTE_MBUF_METADATA_UINT8_PTR(mbuf,			\
			APP_METADATA_OFFSET(32));			\
	memset(key, 0, 32);						\
	k32 = (uint32_t *) key;						\
	k32[0] = (value);						\
	*signature = pipeline_test_hash(key, 0, 0);			\
} while (0)

unsigned n_table_tests = RTE_DIM(table_tests);

/* Function prototypes */
static int
test_table_hash_lru_generic(struct rte_table_ops *ops);
static int
test_table_hash_ext_generic(struct rte_table_ops *ops);

struct rte_bucket_4_8 {
	/* Cache line 0 */
	uint64_t signature;
	uint64_t lru_list;
	struct rte_bucket_4_8 *next;
	uint64_t next_valid;
	uint64_t key[4];
	/* Cache line 1 */
	uint8_t data[0];
};

#if RTE_TABLE_HASH_LRU_STRATEGY == 3
uint64_t shuffles = 0xfffffffdfffbfff9ULL;
#else
uint64_t shuffles = 0x0003000200010000ULL;
#endif

static int test_lru_update(void)
{
	struct rte_bucket_4_8 b;
	struct rte_bucket_4_8 *bucket;
	uint32_t i;
	uint64_t pos;
	uint64_t iterations;
	uint64_t j;
	int poss;

	printf("---------------------------\n");
	printf("Testing lru_update macro...\n");
	printf("---------------------------\n");
	bucket = &b;
	iterations = 10;
#if RTE_TABLE_HASH_LRU_STRATEGY == 3
	bucket->lru_list = 0xFFFFFFFFFFFFFFFFULL;
#else
	bucket->lru_list = 0x0000000100020003ULL;
#endif
	poss = 0;
	for (j = 0; j < iterations; j++)
		for (i = 0; i < 9; i++) {
			uint32_t idx = i >> 1;
			lru_update(bucket, idx);
			pos = lru_pos(bucket);
			poss += pos;
			printf("%s: %d lru_list=%016"PRIx64", upd=%d, "
				"pos=%"PRIx64"\n",
				__func__, i, bucket->lru_list, i>>1, pos);
		}

	if (bucket->lru_list != shuffles) {
		printf("%s: ERROR: %d lru_list=%016"PRIx64", expected %016"
			PRIx64"\n",
			__func__, i, bucket->lru_list, shuffles);
		return -1;
	}
	printf("%s: output checksum of results =%d\n",
		__func__, poss);
#if 0
	if (poss != 126) {
		printf("%s: ERROR output checksum of results =%d expected %d\n",
			__func__, poss, 126);
		return -1;
	}
#endif

	fflush(stdout);

	uint64_t sc_start = rte_rdtsc();
	iterations = 100000000;
	poss = 0;
	for (j = 0; j < iterations; j++) {
		for (i = 0; i < 4; i++) {
			lru_update(bucket, i);
			pos |= bucket->lru_list;
		}
	}
	uint64_t sc_end = rte_rdtsc();

	printf("%s: output checksum of results =%llu\n",
		__func__, (long long unsigned int)pos);
	printf("%s: start=%016"PRIx64", end=%016"PRIx64"\n",
		__func__, sc_start, sc_end);
	printf("\nlru_update: %lu cycles per loop iteration.\n\n",
		(long unsigned int)((sc_end-sc_start)/(iterations*4)));

	return 0;
}

/* Table tests */
int
test_table_stub(void)
{
	int i;
	uint64_t expected_mask = 0, result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];

	/* Create */
	table = rte_table_stub_ops.f_create(NULL, 0, 1);
	if (table == NULL)
		return -1;

	/* Traffic flow */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0)
			PREPARE_PACKET(mbufs[i], 0xadadadad);
		else
			PREPARE_PACKET(mbufs[i], 0xadadadab);

	expected_mask = 0;
	rte_table_stub_ops.f_lookup(table, mbufs, -1,
		&result_mask, (void **)entries);
	if (result_mask != expected_mask)
		return -2;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	return 0;
}

int
test_table_array(void)
{
	int status, i;
	uint64_t result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	char entry1, entry2;
	void *entry_ptr;
	int key_found;

	/* Initialize params and create tables */
	struct rte_table_array_params array_params = {
		.n_entries = 7,
		.offset = APP_METADATA_OFFSET(1)
	};

	table = rte_table_array_ops.f_create(NULL, 0, 1);
	if (table != NULL)
		return -1;

	array_params.n_entries = 0;

	table = rte_table_array_ops.f_create(&array_params, 0, 1);
	if (table != NULL)
		return -2;

	array_params.n_entries = 7;

	table = rte_table_array_ops.f_create(&array_params, 0, 1);
	if (table != NULL)
		return -3;

	array_params.n_entries = 1 << 24;
	array_params.offset = APP_METADATA_OFFSET(1);

	table = rte_table_array_ops.f_create(&array_params, 0, 1);
	if (table == NULL)
		return -4;

	array_params.offset = APP_METADATA_OFFSET(32);

	table = rte_table_array_ops.f_create(&array_params, 0, 1);
	if (table == NULL)
		return -5;

	/* Free */
	status = rte_table_array_ops.f_free(table);
	if (status < 0)
		return -6;

	status = rte_table_array_ops.f_free(NULL);
	if (status == 0)
		return -7;

	/* Add */
	struct rte_table_array_key array_key_1 = {
		.pos = 10,
	};
	struct rte_table_array_key array_key_2 = {
		.pos = 20,
	};
	entry1 = 'A';
	entry2 = 'B';

	table = rte_table_array_ops.f_create(&array_params, 0, 1);
	if (table == NULL)
		return -8;

	status = rte_table_array_ops.f_add(NULL, (void *) &array_key_1, &entry1,
		&key_found, &entry_ptr);
	if (status == 0)
		return -9;

	status = rte_table_array_ops.f_add(table, (void *) &array_key_1, NULL,
		&key_found, &entry_ptr);
	if (status == 0)
		return -10;

	status = rte_table_array_ops.f_add(table, (void *) &array_key_1,
		&entry1, &key_found, &entry_ptr);
	if (status != 0)
		return -11;

	/* Traffic flow */
	status = rte_table_array_ops.f_add(table, (void *) &array_key_2,
		&entry2, &key_found, &entry_ptr);
	if (status != 0)
		return -12;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0)
			PREPARE_PACKET(mbufs[i], 10);
		else
			PREPARE_PACKET(mbufs[i], 20);

	rte_table_array_ops.f_lookup(table, mbufs, -1,
		&result_mask, (void **)entries);

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0 && *entries[i] != 'A')
			return -13;
		else
			if (i % 2 == 1 && *entries[i] != 'B')
				return -13;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	status = rte_table_array_ops.f_free(table);

	return 0;
}

int
test_table_lpm(void)
{
	int status, i;
	uint64_t expected_mask = 0, result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	char entry;
	void *entry_ptr;
	int key_found;
	uint32_t entry_size = 1;

	/* Initialize params and create tables */
	struct rte_table_lpm_params lpm_params = {
		.name = "LPM",
		.n_rules = 1 << 24,
		.number_tbl8s = 1 << 8,
		.flags = 0,
		.entry_unique_size = entry_size,
		.offset = APP_METADATA_OFFSET(1)
	};

	table = rte_table_lpm_ops.f_create(NULL, 0, entry_size);
	if (table != NULL)
		return -1;

	lpm_params.name = NULL;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -2;

	lpm_params.name = "LPM";
	lpm_params.n_rules = 0;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -3;

	lpm_params.n_rules = 1 << 24;
	lpm_params.offset = APP_METADATA_OFFSET(32);
	lpm_params.entry_unique_size = 0;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -4;

	lpm_params.entry_unique_size = entry_size + 1;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -5;

	lpm_params.entry_unique_size = entry_size;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, entry_size);
	if (table == NULL)
		return -6;

	/* Free */
	status = rte_table_lpm_ops.f_free(table);
	if (status < 0)
		return -7;

	status = rte_table_lpm_ops.f_free(NULL);
	if (status == 0)
		return -8;

	/* Add */
	struct rte_table_lpm_key lpm_key;
	lpm_key.ip = 0xadadadad;

	table = rte_table_lpm_ops.f_create(&lpm_params, 0, 1);
	if (table == NULL)
		return -9;

	status = rte_table_lpm_ops.f_add(NULL, &lpm_key, &entry, &key_found,
		&entry_ptr);
	if (status == 0)
		return -10;

	status = rte_table_lpm_ops.f_add(table, NULL, &entry, &key_found,
		&entry_ptr);
	if (status == 0)
		return -11;

	status = rte_table_lpm_ops.f_add(table, &lpm_key, NULL, &key_found,
		&entry_ptr);
	if (status == 0)
		return -12;

	lpm_key.depth = 0;
	status = rte_table_lpm_ops.f_add(table, &lpm_key, &entry, &key_found,
		&entry_ptr);
	if (status == 0)
		return -13;

	lpm_key.depth = 33;
	status = rte_table_lpm_ops.f_add(table, &lpm_key, &entry, &key_found,
		&entry_ptr);
	if (status == 0)
		return -14;

	lpm_key.depth = 16;
	status = rte_table_lpm_ops.f_add(table, &lpm_key, &entry, &key_found,
		&entry_ptr);
	if (status != 0)
		return -15;

	/* Delete */
	status = rte_table_lpm_ops.f_delete(NULL, &lpm_key, &key_found, NULL);
	if (status == 0)
		return -16;

	status = rte_table_lpm_ops.f_delete(table, NULL, &key_found, NULL);
	if (status == 0)
		return -17;

	lpm_key.depth = 0;
	status = rte_table_lpm_ops.f_delete(table, &lpm_key, &key_found, NULL);
	if (status == 0)
		return -18;

	lpm_key.depth = 33;
	status = rte_table_lpm_ops.f_delete(table, &lpm_key, &key_found, NULL);
	if (status == 0)
		return -19;

	lpm_key.depth = 16;
	status = rte_table_lpm_ops.f_delete(table, &lpm_key, &key_found, NULL);
	if (status != 0)
		return -20;

	status = rte_table_lpm_ops.f_delete(table, &lpm_key, &key_found, NULL);
	if (status != 0)
		return -21;

	/* Traffic flow */
	entry = 'A';
	status = rte_table_lpm_ops.f_add(table, &lpm_key, &entry, &key_found,
		&entry_ptr);
	if (status < 0)
		return -22;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0) {
			expected_mask |= (uint64_t)1 << i;
			PREPARE_PACKET(mbufs[i], 0xadadadad);
		} else
			PREPARE_PACKET(mbufs[i], 0xadadadab);

	rte_table_lpm_ops.f_lookup(table, mbufs, -1,
		&result_mask, (void **)entries);
	if (result_mask != expected_mask)
		return -23;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	status = rte_table_lpm_ops.f_free(table);

	return 0;
}

int
test_table_lpm_ipv6(void)
{
	int status, i;
	uint64_t expected_mask = 0, result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	char entry;
	void *entry_ptr;
	int key_found;
	uint32_t entry_size = 1;

	/* Initialize params and create tables */
	struct rte_table_lpm_ipv6_params lpm_params = {
		.name = "LPM",
		.n_rules = 1 << 24,
		.number_tbl8s = 1 << 21,
		.entry_unique_size = entry_size,
		.offset = APP_METADATA_OFFSET(32)
	};

	table = rte_table_lpm_ipv6_ops.f_create(NULL, 0, entry_size);
	if (table != NULL)
		return -1;

	lpm_params.name = NULL;

	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -2;

	lpm_params.name = "LPM";
	lpm_params.n_rules = 0;

	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -3;

	lpm_params.n_rules = 1 << 24;
	lpm_params.number_tbl8s = 0;
	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -4;

	lpm_params.number_tbl8s = 1 << 21;
	lpm_params.entry_unique_size = 0;
	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -5;

	lpm_params.entry_unique_size = entry_size + 1;
	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table != NULL)
		return -6;

	lpm_params.entry_unique_size = entry_size;
	lpm_params.offset = APP_METADATA_OFFSET(32);

	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table == NULL)
		return -7;

	/* Free */
	status = rte_table_lpm_ipv6_ops.f_free(table);
	if (status < 0)
		return -8;

	status = rte_table_lpm_ipv6_ops.f_free(NULL);
	if (status == 0)
		return -9;

	/* Add */
	struct rte_table_lpm_ipv6_key lpm_key;

	lpm_key.ip[0] = 0xad;
	lpm_key.ip[1] = 0xad;
	lpm_key.ip[2] = 0xad;
	lpm_key.ip[3] = 0xad;

	table = rte_table_lpm_ipv6_ops.f_create(&lpm_params, 0, entry_size);
	if (table == NULL)
		return -10;

	status = rte_table_lpm_ipv6_ops.f_add(NULL, &lpm_key, &entry,
		&key_found, &entry_ptr);
	if (status == 0)
		return -11;

	status = rte_table_lpm_ipv6_ops.f_add(table, NULL, &entry, &key_found,
		&entry_ptr);
	if (status == 0)
		return -12;

	status = rte_table_lpm_ipv6_ops.f_add(table, &lpm_key, NULL, &key_found,
		&entry_ptr);
	if (status == 0)
		return -13;

	lpm_key.depth = 0;
	status = rte_table_lpm_ipv6_ops.f_add(table, &lpm_key, &entry,
		&key_found, &entry_ptr);
	if (status == 0)
		return -14;

	lpm_key.depth = 129;
	status = rte_table_lpm_ipv6_ops.f_add(table, &lpm_key, &entry,
		&key_found, &entry_ptr);
	if (status == 0)
		return -15;

	lpm_key.depth = 16;
	status = rte_table_lpm_ipv6_ops.f_add(table, &lpm_key, &entry,
		&key_found, &entry_ptr);
	if (status != 0)
		return -16;

	/* Delete */
	status = rte_table_lpm_ipv6_ops.f_delete(NULL, &lpm_key, &key_found,
		NULL);
	if (status == 0)
		return -17;

	status = rte_table_lpm_ipv6_ops.f_delete(table, NULL, &key_found, NULL);
	if (status == 0)
		return -18;

	lpm_key.depth = 0;
	status = rte_table_lpm_ipv6_ops.f_delete(table, &lpm_key, &key_found,
		NULL);
	if (status == 0)
		return -19;

	lpm_key.depth = 129;
	status = rte_table_lpm_ipv6_ops.f_delete(table, &lpm_key, &key_found,
		NULL);
	if (status == 0)
		return -20;

	lpm_key.depth = 16;
	status = rte_table_lpm_ipv6_ops.f_delete(table, &lpm_key, &key_found,
		NULL);
	if (status != 0)
		return -21;

	status = rte_table_lpm_ipv6_ops.f_delete(table, &lpm_key, &key_found,
		NULL);
	if (status != 0)
		return -22;

	/* Traffic flow */
	entry = 'A';
	status = rte_table_lpm_ipv6_ops.f_add(table, &lpm_key, &entry,
		&key_found, &entry_ptr);
	if (status < 0)
		return -23;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0) {
			expected_mask |= (uint64_t)1 << i;
			PREPARE_PACKET(mbufs[i], 0xadadadad);
		} else
			PREPARE_PACKET(mbufs[i], 0xadadadab);

	rte_table_lpm_ipv6_ops.f_lookup(table, mbufs, -1,
		&result_mask, (void **)entries);
	if (result_mask != expected_mask)
		return -24;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	status = rte_table_lpm_ipv6_ops.f_free(table);

	return 0;
}

static int
test_table_hash_lru_generic(struct rte_table_ops *ops)
{
	int status, i;
	uint64_t expected_mask = 0, result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	char entry;
	void *entry_ptr;
	int key_found;

	/* Initialize params and create tables */
	struct rte_table_hash_key8_lru_params hash_params = {
		.n_entries = 1 << 10,
		.f_hash = pipeline_test_hash,
		.seed = 0,
		.signature_offset = APP_METADATA_OFFSET(1),
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
	};

	hash_params.n_entries = 0;

	table = ops->f_create(&hash_params, 0, 1);
	if (table != NULL)
		return -1;

	hash_params.n_entries = 1 << 10;
	hash_params.signature_offset = APP_METADATA_OFFSET(1);

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -2;

	hash_params.signature_offset = APP_METADATA_OFFSET(0);
	hash_params.key_offset = APP_METADATA_OFFSET(1);

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -3;

	hash_params.key_offset = APP_METADATA_OFFSET(32);
	hash_params.f_hash = NULL;

	table = ops->f_create(&hash_params, 0, 1);
	if (table != NULL)
		return -4;

	hash_params.f_hash = pipeline_test_hash;

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -5;

	/* Free */
	status = ops->f_free(table);
	if (status < 0)
		return -6;

	status = ops->f_free(NULL);
	if (status == 0)
		return -7;

	/* Add */
	uint8_t key[32];
	uint32_t *k32 = (uint32_t *) &key;

	memset(key, 0, 32);
	k32[0] = rte_be_to_cpu_32(0xadadadad);

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -8;

	entry = 'A';
	status = ops->f_add(table, &key, &entry, &key_found, &entry_ptr);
	if (status != 0)
		return -9;

	/* Delete */
	status = ops->f_delete(table, &key, &key_found, NULL);
	if (status != 0)
		return -10;

	status = ops->f_delete(table, &key, &key_found, NULL);
	if (status != 0)
		return -11;

	/* Traffic flow */
	entry = 'A';
	status = ops->f_add(table, &key, &entry, &key_found, &entry_ptr);
	if (status < 0)
		return -12;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0) {
			expected_mask |= (uint64_t)1 << i;
			PREPARE_PACKET(mbufs[i], 0xadadadad);
		} else
			PREPARE_PACKET(mbufs[i], 0xadadadab);

	ops->f_lookup(table, mbufs, -1, &result_mask, (void **)entries);
	if (result_mask != expected_mask)
		return -13;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	status = ops->f_free(table);

	return 0;
}

static int
test_table_hash_ext_generic(struct rte_table_ops *ops)
{
	int status, i;
	uint64_t expected_mask = 0, result_mask;
	struct rte_mbuf *mbufs[RTE_PORT_IN_BURST_SIZE_MAX];
	void *table;
	char *entries[RTE_PORT_IN_BURST_SIZE_MAX];
	char entry;
	int key_found;
	void *entry_ptr;

	/* Initialize params and create tables */
	struct rte_table_hash_key8_ext_params hash_params = {
		.n_entries = 1 << 10,
		.n_entries_ext = 1 << 4,
		.f_hash = pipeline_test_hash,
		.seed = 0,
		.signature_offset = APP_METADATA_OFFSET(1),
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
	};

	hash_params.n_entries = 0;

	table = ops->f_create(&hash_params, 0, 1);
	if (table != NULL)
		return -1;

	hash_params.n_entries = 1 << 10;
	hash_params.n_entries_ext = 0;
	table = ops->f_create(&hash_params, 0, 1);
	if (table != NULL)
		return -2;

	hash_params.n_entries_ext = 1 << 4;
	hash_params.signature_offset = APP_METADATA_OFFSET(1);
	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -2;

	hash_params.signature_offset = APP_METADATA_OFFSET(0);
	hash_params.key_offset = APP_METADATA_OFFSET(1);

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -3;

	hash_params.key_offset = APP_METADATA_OFFSET(32);
	hash_params.f_hash = NULL;

	table = ops->f_create(&hash_params, 0, 1);
	if (table != NULL)
		return -4;

	hash_params.f_hash = pipeline_test_hash;

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -5;

	/* Free */
	status = ops->f_free(table);
	if (status < 0)
		return -6;

	status = ops->f_free(NULL);
	if (status == 0)
		return -7;

	/* Add */
	uint8_t key[32];
	uint32_t *k32 = (uint32_t *) &key;

	memset(key, 0, 32);
	k32[0] = rte_be_to_cpu_32(0xadadadad);

	table = ops->f_create(&hash_params, 0, 1);
	if (table == NULL)
		return -8;

	entry = 'A';
	status = ops->f_add(table, &key, &entry, &key_found, &entry_ptr);
	if (status != 0)
		return -9;

	/* Delete */
	status = ops->f_delete(table, &key, &key_found, NULL);
	if (status != 0)
		return -10;

	status = ops->f_delete(table, &key, &key_found, NULL);
	if (status != 0)
		return -11;

	/* Traffic flow */
	entry = 'A';
	status = ops->f_add(table, &key, &entry, &key_found, &entry_ptr);
	if (status < 0)
		return -12;

	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		if (i % 2 == 0) {
			expected_mask |= (uint64_t)1 << i;
			PREPARE_PACKET(mbufs[i], 0xadadadad);
		} else
			PREPARE_PACKET(mbufs[i], 0xadadadab);

	ops->f_lookup(table, mbufs, -1, &result_mask, (void **)entries);
	if (result_mask != expected_mask)
		return -13;

	/* Free resources */
	for (i = 0; i < RTE_PORT_IN_BURST_SIZE_MAX; i++)
		rte_pktmbuf_free(mbufs[i]);

	status = ops->f_free(table);

	return 0;
}

int
test_table_hash_lru(void)
{
	int status;

	status = test_table_hash_lru_generic(&rte_table_hash_key8_lru_ops);
	if (status < 0)
		return status;

	status = test_table_hash_lru_generic(
		&rte_table_hash_key8_lru_dosig_ops);
	if (status < 0)
		return status;

	status = test_table_hash_lru_generic(&rte_table_hash_key16_lru_ops);
	if (status < 0)
		return status;

	status = test_table_hash_lru_generic(&rte_table_hash_key32_lru_ops);
	if (status < 0)
		return status;

	status = test_lru_update();
	if (status < 0)
		return status;

	return 0;
}

int
test_table_hash_ext(void)
{
	int status;

	status = test_table_hash_ext_generic(&rte_table_hash_key8_ext_ops);
	if (status < 0)
		return status;

	status = test_table_hash_ext_generic(
		&rte_table_hash_key8_ext_dosig_ops);
	if (status < 0)
		return status;

	status = test_table_hash_ext_generic(&rte_table_hash_key16_ext_ops);
	if (status < 0)
		return status;

	status = test_table_hash_ext_generic(&rte_table_hash_key32_ext_ops);
	if (status < 0)
		return status;

	return 0;
}
