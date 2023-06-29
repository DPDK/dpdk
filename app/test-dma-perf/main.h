/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef MAIN_H
#define MAIN_H


#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>

#define MAX_WORKER_NB 128
#define MAX_OUTPUT_STR_LEN 512

#define MAX_DMA_NB 128
#define MAX_LCORE_NB 256

extern char output_str[MAX_WORKER_NB + 1][MAX_OUTPUT_STR_LEN];

typedef enum {
	OP_NONE = 0,
	OP_ADD,
	OP_MUL
} alg_op_type;

struct test_configure_entry {
	uint32_t first;
	uint32_t last;
	uint32_t incr;
	alg_op_type op;
	uint32_t cur;
};

struct lcore_dma_map_t {
	uint32_t lcores[MAX_WORKER_NB];
	char dma_names[MAX_WORKER_NB][RTE_DEV_NAME_MAX_LEN];
	int16_t dma_ids[MAX_WORKER_NB];
	uint16_t cnt;
};

struct test_configure {
	bool is_valid;
	uint8_t test_type;
	const char *test_type_str;
	uint16_t src_numa_node;
	uint16_t dst_numa_node;
	uint16_t opcode;
	bool is_dma;
	struct lcore_dma_map_t lcore_dma_map;
	struct test_configure_entry mem_size;
	struct test_configure_entry buf_size;
	struct test_configure_entry ring_size;
	struct test_configure_entry kick_batch;
	uint8_t cache_flush;
	uint32_t nr_buf;
	uint16_t test_secs;
	const char *eal_args;
	uint8_t scenario_id;
};

void mem_copy_benchmark(struct test_configure *cfg, bool is_dma);

#endif /* MAIN_H */
