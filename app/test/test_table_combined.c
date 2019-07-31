/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <string.h>
#include "test_table_combined.h"
#include "test_table.h"
#include <rte_table_lpm_ipv6.h>

#define MAX_TEST_KEYS 128
#define N_PACKETS 50

enum check_table_result {
	CHECK_TABLE_OK,
	CHECK_TABLE_PORT_CONFIG,
	CHECK_TABLE_PORT_ENABLE,
	CHECK_TABLE_TABLE_CONFIG,
	CHECK_TABLE_ENTRY_ADD,
	CHECK_TABLE_DEFAULT_ENTRY_ADD,
	CHECK_TABLE_CONNECT,
	CHECK_TABLE_MANAGE_ERROR,
	CHECK_TABLE_CONSISTENCY,
	CHECK_TABLE_NO_TRAFFIC,
	CHECK_TABLE_INVALID_PARAMETER,
};

struct table_packets {
	uint32_t hit_packet[MAX_TEST_KEYS];
	uint32_t miss_packet[MAX_TEST_KEYS];
	uint32_t n_hit_packets;
	uint32_t n_miss_packets;
};

combined_table_test table_tests_combined[] = {
	test_table_lpm_combined,
	test_table_lpm_ipv6_combined,
	test_table_hash8lru,
	test_table_hash8ext,
	test_table_hash16lru,
	test_table_hash16ext,
	test_table_hash32lru,
	test_table_hash32ext,
	test_table_hash_cuckoo_combined,
};

unsigned n_table_tests_combined = RTE_DIM(table_tests_combined);

/* Generic port tester function */
static int
test_table_type(struct rte_table_ops *table_ops, void *table_args,
	void *key, struct table_packets *table_packets,
	struct manage_ops *manage_ops, unsigned n_ops)
{
	uint32_t ring_in_id, table_id, ring_out_id, ring_out_2_id;
	unsigned i;

	RTE_SET_USED(manage_ops);
	RTE_SET_USED(n_ops);
	/* Create pipeline */
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = 0,
	};

	struct rte_pipeline *pipeline = rte_pipeline_create(&pipeline_params);

	/* Create input ring */
	struct rte_port_ring_reader_params ring_params_rx = {
		.ring = RING_RX,
	};

	struct rte_port_ring_writer_params ring_params_tx = {
		.ring = RING_RX,
		.tx_burst_sz = RTE_PORT_IN_BURST_SIZE_MAX,
	};

	struct rte_pipeline_port_in_params ring_in_params = {
		.ops = &rte_port_ring_reader_ops,
		.arg_create = (void *)&ring_params_rx,
		.f_action = NULL,
		.burst_size = RTE_PORT_IN_BURST_SIZE_MAX,
	};

	if (rte_pipeline_port_in_create(pipeline, &ring_in_params,
		&ring_in_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_PORT_CONFIG;
	}

	/* Create table */
	struct rte_pipeline_table_params table_params = {
		.ops = table_ops,
		.arg_create = table_args,
		.f_action_hit = NULL,
		.f_action_miss = NULL,
		.arg_ah = NULL,
		.action_data_size = 0,
	};

	if (rte_pipeline_table_create(pipeline, &table_params,
		&table_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_TABLE_CONFIG;
	}

	/* Create output ports */
	ring_params_tx.ring = RING_TX;

	struct rte_pipeline_port_out_params ring_out_params = {
		.ops = &rte_port_ring_writer_ops,
		.arg_create = (void *)&ring_params_tx,
		.f_action = NULL,
	};

	if (rte_pipeline_port_out_create(pipeline, &ring_out_params,
		&ring_out_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_PORT_CONFIG;
	}

	ring_params_tx.ring = RING_TX_2;

	if (rte_pipeline_port_out_create(pipeline, &ring_out_params,
		&ring_out_2_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_PORT_CONFIG;
	}

	/* Add entry to the table */
	struct rte_pipeline_table_entry default_entry = {
		.action = RTE_PIPELINE_ACTION_DROP,
		{.table_id = ring_out_id},
	};

	struct rte_pipeline_table_entry table_entry = {
		.action = RTE_PIPELINE_ACTION_PORT,
		{.table_id = ring_out_id},
	};

	struct rte_pipeline_table_entry *default_entry_ptr, *entry_ptr;

	int key_found;

	if (rte_pipeline_table_default_entry_add(pipeline, table_id,
		&default_entry, &default_entry_ptr) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_DEFAULT_ENTRY_ADD;
	}

	if (rte_pipeline_table_entry_add(pipeline, table_id,
		key ? key : &table_entry, &table_entry, &key_found,
			&entry_ptr) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_ENTRY_ADD;
	}

	/* Create connections and check consistency */
	if (rte_pipeline_port_in_connect_to_table(pipeline, ring_in_id,
		table_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_CONNECT;
	}

	if (rte_pipeline_port_in_enable(pipeline, ring_in_id) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_PORT_ENABLE;
	}

	if (rte_pipeline_check(pipeline) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_CONSISTENCY;
	}



	/* Flow test - All hits */
	if (table_packets->n_hit_packets) {
		for (i = 0; i < table_packets->n_hit_packets; i++)
			RING_ENQUEUE(RING_RX, table_packets->hit_packet[i]);

		RUN_PIPELINE(pipeline);

		VERIFY_TRAFFIC(RING_TX, table_packets->n_hit_packets,
				table_packets->n_hit_packets);
	}

	/* Flow test - All misses */
	if (table_packets->n_miss_packets) {
		for (i = 0; i < table_packets->n_miss_packets; i++)
			RING_ENQUEUE(RING_RX, table_packets->miss_packet[i]);

		RUN_PIPELINE(pipeline);

		VERIFY_TRAFFIC(RING_TX, table_packets->n_miss_packets, 0);
	}

	/* Flow test - Half hits, half misses */
	if (table_packets->n_hit_packets && table_packets->n_miss_packets) {
		for (i = 0; i < (table_packets->n_hit_packets) / 2; i++)
			RING_ENQUEUE(RING_RX, table_packets->hit_packet[i]);

		for (i = 0; i < (table_packets->n_miss_packets) / 2; i++)
			RING_ENQUEUE(RING_RX, table_packets->miss_packet[i]);

		RUN_PIPELINE(pipeline);
		VERIFY_TRAFFIC(RING_TX, table_packets->n_hit_packets,
			table_packets->n_hit_packets / 2);
	}

	/* Flow test - Single packet */
	if (table_packets->n_hit_packets) {
		RING_ENQUEUE(RING_RX, table_packets->hit_packet[0]);
		RUN_PIPELINE(pipeline);
		VERIFY_TRAFFIC(RING_TX, table_packets->n_hit_packets, 1);
	}
	if (table_packets->n_miss_packets) {
		RING_ENQUEUE(RING_RX, table_packets->miss_packet[0]);
		RUN_PIPELINE(pipeline);
		VERIFY_TRAFFIC(RING_TX, table_packets->n_miss_packets, 0);
	}


	/* Change table entry action */
	printf("Change entry action\n");
	table_entry.table_id = ring_out_2_id;

	if (rte_pipeline_table_default_entry_add(pipeline, table_id,
		&default_entry, &default_entry_ptr) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_ENTRY_ADD;
	}

	if (rte_pipeline_table_entry_add(pipeline, table_id,
		key ? key : &table_entry, &table_entry, &key_found,
			&entry_ptr) != 0) {
		rte_pipeline_free(pipeline);
		return -CHECK_TABLE_ENTRY_ADD;
	}

	/* Check that traffic destination has changed */
	if (table_packets->n_hit_packets) {
		for (i = 0; i < table_packets->n_hit_packets; i++)
			RING_ENQUEUE(RING_RX, table_packets->hit_packet[i]);

		RUN_PIPELINE(pipeline);
		VERIFY_TRAFFIC(RING_TX, table_packets->n_hit_packets, 0);
		VERIFY_TRAFFIC(RING_TX_2, table_packets->n_hit_packets,
			table_packets->n_hit_packets);
	}

	printf("delete entry\n");
	/* Delete table entry */
	rte_pipeline_table_entry_delete(pipeline, table_id,
		key ? key : &table_entry, &key_found, NULL);

	rte_pipeline_free(pipeline);

	return 0;
}

/* Table tests */
int
test_table_stub_combined(void)
{
	int status, i;
	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < N_PACKETS; i++)
		table_packets.hit_packet[i] = i;

	table_packets.n_hit_packets = N_PACKETS;
	table_packets.n_miss_packets = 0;

	status = test_table_type(&rte_table_stub_ops, NULL, NULL,
		&table_packets, NULL, 1);
	VERIFY(status, CHECK_TABLE_OK);

	return 0;
}

int
test_table_lpm_combined(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_lpm_params lpm_params = {
		.name = "LPM",
		.n_rules = 1 << 16,
		.number_tbl8s = 1 << 8,
		.flags = 0,
		.entry_unique_size = 8,
		.offset = APP_METADATA_OFFSET(0),
	};

	struct rte_table_lpm_key lpm_key = {
		.ip = 0xadadadad,
		.depth = 16,
	};

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");

	for (i = 0; i < N_PACKETS; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < N_PACKETS; i++)
		table_packets.miss_packet[i] = 0xfefefefe;

	table_packets.n_hit_packets = N_PACKETS;
	table_packets.n_miss_packets = N_PACKETS;

	status = test_table_type(&rte_table_lpm_ops, (void *)&lpm_params,
		(void *)&lpm_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	lpm_params.n_rules = 0;

	status = test_table_type(&rte_table_lpm_ops, (void *)&lpm_params,
		(void *)&lpm_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	lpm_params.n_rules = 1 << 24;
	lpm_key.depth = 0;

	status = test_table_type(&rte_table_lpm_ops, (void *)&lpm_params,
		(void *)&lpm_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_ENTRY_ADD);

	lpm_key.depth = 33;

	status = test_table_type(&rte_table_lpm_ops, (void *)&lpm_params,
		(void *)&lpm_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_ENTRY_ADD);

	return 0;
}

int
test_table_lpm_ipv6_combined(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_lpm_ipv6_params lpm_ipv6_params = {
		.name = "LPM",
		.n_rules = 1 << 16,
		.number_tbl8s = 1 << 13,
		.entry_unique_size = 8,
		.offset = APP_METADATA_OFFSET(32),
	};

	struct rte_table_lpm_ipv6_key lpm_ipv6_key = {
		.depth = 16,
	};
	memset(lpm_ipv6_key.ip, 0xad, 16);

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < N_PACKETS; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < N_PACKETS; i++)
		table_packets.miss_packet[i] = 0xadadadab;

	table_packets.n_hit_packets = N_PACKETS;
	table_packets.n_miss_packets = N_PACKETS;

	status = test_table_type(&rte_table_lpm_ipv6_ops,
		(void *)&lpm_ipv6_params,
		(void *)&lpm_ipv6_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	lpm_ipv6_params.n_rules = 0;

	status = test_table_type(&rte_table_lpm_ipv6_ops,
		(void *)&lpm_ipv6_params,
		(void *)&lpm_ipv6_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	lpm_ipv6_params.n_rules = 1 << 24;
	lpm_ipv6_key.depth = 0;

	status = test_table_type(&rte_table_lpm_ipv6_ops,
		(void *)&lpm_ipv6_params,
		(void *)&lpm_ipv6_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_ENTRY_ADD);

	lpm_ipv6_key.depth = 129;
	status = test_table_type(&rte_table_lpm_ipv6_ops,
		(void *)&lpm_ipv6_params,
		(void *)&lpm_ipv6_key, &table_packets, NULL, 0);
	VERIFY(status, CHECK_TABLE_ENTRY_ADD);

	return 0;
}

int
test_table_hash8lru(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key8lru_params = {
		.name = "TABLE",
		.key_size = 8,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key8lru[8];
	uint32_t *k8lru = (uint32_t *) key8lru;

	memset(key8lru, 0, sizeof(key8lru));
	k8lru[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xfefefefe;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key8_lru_ops,
		(void *)&key8lru_params, (void *)key8lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key8lru_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key8_lru_ops,
		(void *)&key8lru_params, (void *)key8lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key8lru_params.n_keys = 1<<16;
	key8lru_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key8_lru_ops,
		(void *)&key8lru_params, (void *)key8lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash16lru(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key16lru_params = {
		.name = "TABLE",
		.key_size = 16,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key16lru[16];
	uint32_t *k16lru = (uint32_t *) key16lru;

	memset(key16lru, 0, sizeof(key16lru));
	k16lru[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xfefefefe;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key16_lru_ops,
		(void *)&key16lru_params, (void *)key16lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key16lru_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key16_lru_ops,
		(void *)&key16lru_params, (void *)key16lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key16lru_params.n_keys = 1<<16;
	key16lru_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key16_lru_ops,
		(void *)&key16lru_params, (void *)key16lru, &table_packets,
			NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash32lru(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key32lru_params = {
		.name = "TABLE",
		.key_size = 32,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key32lru[32];
	uint32_t *k32lru = (uint32_t *) key32lru;

	memset(key32lru, 0, sizeof(key32lru));
	k32lru[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xbdadadad;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key32_lru_ops,
		(void *)&key32lru_params, (void *)key32lru, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key32lru_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key32_lru_ops,
		(void *)&key32lru_params, (void *)key32lru, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key32lru_params.n_keys = 1<<16;
	key32lru_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key32_lru_ops,
		(void *)&key32lru_params, (void *)key32lru, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash8ext(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key8ext_params = {
		.name = "TABLE",
		.key_size = 8,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key8ext[8];
	uint32_t *k8ext = (uint32_t *) key8ext;

	memset(key8ext, 0, sizeof(key8ext));
	k8ext[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xbdadadad;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key8_ext_ops,
		(void *)&key8ext_params, (void *)key8ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key8ext_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key8_ext_ops,
		(void *)&key8ext_params, (void *)key8ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key8ext_params.n_keys = 1<<16;
	key8ext_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key8_ext_ops,
		(void *)&key8ext_params, (void *)key8ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash16ext(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key16ext_params = {
		.name = "TABLE",
		.key_size = 16,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key16ext[16];
	uint32_t *k16ext = (uint32_t *) key16ext;

	memset(key16ext, 0, sizeof(key16ext));
	k16ext[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xbdadadad;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key16_ext_ops,
		(void *)&key16ext_params, (void *)key16ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key16ext_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key16_ext_ops,
		(void *)&key16ext_params, (void *)key16ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key16ext_params.n_keys = 1<<16;
	key16ext_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key16_ext_ops,
		(void *)&key16ext_params, (void *)key16ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash32ext(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_params key32ext_params = {
		.name = "TABLE",
		.key_size = 32,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash,
		.seed = 0,
	};

	uint8_t key32ext[32];
	uint32_t *k32ext = (uint32_t *) key32ext;

	memset(key32ext, 0, sizeof(key32ext));
	k32ext[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xbdadadad;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_key32_ext_ops,
		(void *)&key32ext_params, (void *)key32ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	key32ext_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_key32_ext_ops,
		(void *)&key32ext_params, (void *)key32ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	key32ext_params.n_keys = 1<<16;
	key32ext_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_key32_ext_ops,
		(void *)&key32ext_params, (void *)key32ext, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}

int
test_table_hash_cuckoo_combined(void)
{
	int status, i;

	/* Traffic flow */
	struct rte_table_hash_cuckoo_params cuckoo_params = {
		.name = "TABLE",
		.key_size = 32,
		.key_offset = APP_METADATA_OFFSET(32),
		.key_mask = NULL,
		.n_keys = 1 << 16,
		.n_buckets = 1 << 16,
		.f_hash = pipeline_test_hash_cuckoo,
		.seed = 0,
	};

	uint8_t key_cuckoo[32];
	uint32_t *kcuckoo = (uint32_t *) key_cuckoo;

	memset(key_cuckoo, 0, sizeof(key_cuckoo));
	kcuckoo[0] = 0xadadadad;

	struct table_packets table_packets;

	printf("--------------\n");
	printf("RUNNING TEST - %s\n", __func__);
	printf("--------------\n");
	for (i = 0; i < 50; i++)
		table_packets.hit_packet[i] = 0xadadadad;

	for (i = 0; i < 50; i++)
		table_packets.miss_packet[i] = 0xbdadadad;

	table_packets.n_hit_packets = 50;
	table_packets.n_miss_packets = 50;

	status = test_table_type(&rte_table_hash_cuckoo_ops,
		(void *)&cuckoo_params, (void *)key_cuckoo, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_OK);

	/* Invalid parameters */
	cuckoo_params.key_size = 0;

	status = test_table_type(&rte_table_hash_cuckoo_ops,
		(void *)&cuckoo_params, (void *)key_cuckoo, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	cuckoo_params.key_size = 32;
	cuckoo_params.n_keys = 0;

	status = test_table_type(&rte_table_hash_cuckoo_ops,
		(void *)&cuckoo_params, (void *)key_cuckoo, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	cuckoo_params.n_keys = 1<<16;
	cuckoo_params.f_hash = NULL;

	status = test_table_type(&rte_table_hash_cuckoo_ops,
		(void *)&cuckoo_params, (void *)key_cuckoo, &table_packets,
		NULL, 0);
	VERIFY(status, CHECK_TABLE_TABLE_CONFIG);

	return 0;
}
