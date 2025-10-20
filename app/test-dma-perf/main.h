/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef MAIN_H
#define MAIN_H


#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_dev.h>

#define MAX_WORKER_NB 128

#define MAX_DMA_NB 128

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
	char dma_names[RTE_DEV_NAME_MAX_LEN];
	uint32_t lcore;
	int16_t dma_id;
};

struct vchan_dev_config {
	struct rte_dma_port_param port;
	uintptr_t raddr;
	uint8_t tdir;
};

struct lcore_dma_config {
	struct lcore_dma_map_t lcore_dma_map;
	struct vchan_dev_config vchan_dev;
};

struct test_configure {
	bool is_valid;
	bool is_skip;
	uint8_t test_type;
	const char *test_type_str;
	uint16_t src_numa_node;
	uint16_t dst_numa_node;
	uint16_t opcode;
	bool is_dma;
	bool is_sg;
	bool use_ops;
	struct lcore_dma_config dma_config[MAX_WORKER_NB];
	struct test_configure_entry mem_size;
	struct test_configure_entry buf_size;
	struct test_configure_entry ring_size;
	struct test_configure_entry kick_batch;
	uint16_t num_worker;
	uint8_t nb_src_sges;
	uint8_t nb_dst_sges;
	uint32_t nr_buf;
	uint8_t scenario_id;
};

#define MAX_EAL_ARGV_NB 100

struct global_configure {
	char *eal_argv[MAX_EAL_ARGV_NB + 1];
	int   eal_argc;
	uint8_t cache_flush;
	uint16_t test_secs;
};

extern struct global_configure global_cfg;

void output_csv(const char *fmt, ...);
int mem_copy_benchmark(struct test_configure *cfg);

#endif /* MAIN_H */
